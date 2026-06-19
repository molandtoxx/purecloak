// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/anti_detection/anti_detection_engine.h"

#include <string>

#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace purecloak {

// static
std::string AntiDetectionEngine::GeneratePRNGFunction(int seed) {
  return base::StringPrintf(
      "function _pcNoise(base,index){"
      "var s=%d;"
      "var x=(s*0x9E3779B9+index)&0xFFFFFFFF;"
      "var y=((x^(x>>>16))*0x85EBCA6B)&0xFFFFFFFF;"
      "var z=((y^(y>>>13))*0xC2B2AE35)&0xFFFFFFFF;"
      "var delta=((z&0xFF)-128);"
      "return Math.max(0,Math.min(255,base+delta));"
      "}",
      seed);
}

std::string AntiDetectionEngine::GenerateProtectionScript(
    const Workspace& workspace) const {
  if (workspace.fingerprint_seed == 0) {
    return "";
  }

  int seed = workspace.fingerprint_seed;
  std::string script;

  script += "(function(){";
  script += GeneratePRNGFunction(seed);
  script += "\n";

  script += GenerateCanvasProtector(seed);
  script += "\n";

  script += GenerateWebGLProtector(workspace);
  script += "\n";

  script += GenerateAudioProtector(seed);
  script += "\n";

  script += GenerateFontProtector(seed);
  script += "\n";

  script += GenerateWebRTCProtector();
  script += "\n";

  script += GenerateWebdriverRemover();
  script += "\n";

  script += "})();";

  return script;
}

std::string AntiDetectionEngine::GenerateCanvasProtector(int seed) const {
  return base::StringPrintf(
      // getImageData: add per-pixel deterministic noise
      "var _origGetImageData=CanvasRenderingContext2D.prototype.getImageData;"
      "CanvasRenderingContext2D.prototype.getImageData=function(){"
      "var d=_origGetImageData.apply(this,arguments);"
      "for(var i=0;i<d.data.length;i+=4){"
      "d.data[i]=_pcNoise(d.data[i],%d+i);"
      "d.data[i+1]=_pcNoise(d.data[i+1],%d+i+1);"
      "d.data[i+2]=_pcNoise(d.data[i+2],%d+i+2);"
      "}"
      "return d;"
      "};"
      // toDataURL: insert invisible noise before serialization
      "var _origToDataURL=HTMLCanvasElement.prototype.toDataURL;"
      "HTMLCanvasElement.prototype.toDataURL=function(){"
      "var ctx=this.getContext('2d');"
      "if(ctx){"
      "try{"
      "var w=this.width,h=this.height;"
      "var img=_origGetImageData.call(ctx,0,0,Math.min(w,1),Math.min(h,1));"
      "if(img&&img.data&&img.data.length>=4){"
      "img.data[0]=_pcNoise(img.data[0],%d);"
      "ctx.putImageData(img,0,0);"
      "}"
      "}catch(e){}"
      "}"
      "return _origToDataURL.apply(this,arguments);"
      "};",
      seed, seed, seed, seed);
}

std::string AntiDetectionEngine::GenerateWebGLProtector(
    const Workspace& workspace) const {
  // Escape vendor/renderer for safe JS string embedding.
  std::string vendor, renderer;
  if (!workspace.gpu_vendor.empty()) {
    base::EscapeJSONString(workspace.gpu_vendor, false, &vendor);
    for (size_t i = 0; i < vendor.size(); ++i) {
      if (vendor[i] == '\'') {
        vendor.insert(i, "\\");
        ++i;
      }
    }
  }
  if (!workspace.gpu_renderer.empty()) {
    base::EscapeJSONString(workspace.gpu_renderer, false, &renderer);
    for (size_t i = 0; i < renderer.size(); ++i) {
      if (renderer[i] == '\'') {
        renderer.insert(i, "\\");
        ++i;
      }
    }
  }

  if (vendor.empty() && renderer.empty()) {
    // No GPU override configured; only filter extensions.
    return base::StringPrintf(
        "var _origGetExt=WebGLRenderingContext.prototype.getSupportedExtensions;"
        "WebGLRenderingContext.prototype.getSupportedExtensions=function(){"
        "var e=_origGetExt.call(this)||[];"
        "return e.filter(function(x){return x.indexOf('WEBGL_debug_renderer')===-1;});"
        "};");
  }

  return base::StringPrintf(
      // WebGL1 getParameter override
      "var _v='%s',_r='%s';"
      "var _origGetParam=WebGLRenderingContext.prototype.getParameter;"
      "WebGLRenderingContext.prototype.getParameter=function(p){"
      "if(p===0x9245)return _v;"
      "if(p===0x9246)return _r;"
      "if(p===0x1F01&&_r)return _r;"
      "if(p===0x1F00&&_v)return _v;"
      "return _origGetParam.call(this,p);"
      "};"
      // UNMASKED_VENDOR_WEBGL = 0x9245
      // UNMASKED_RENDERER_WEBGL = 0x9246
      // WebGL2 getParameter override
      "if(typeof WebGL2RenderingContext!=='undefined'){"
      "var _origGetParam2=WebGL2RenderingContext.prototype.getParameter;"
      "WebGL2RenderingContext.prototype.getParameter=function(p){"
      "if(p===0x9245)return _v;"
      "if(p===0x9246)return _r;"
      "if(p===0x1F01&&_r)return _r;"
      "if(p===0x1F00&&_v)return _v;"
      "return _origGetParam2.call(this,p);"
      "};"
      "}"
      // Filter extensions to hide debug info
      "var _origGetExt=WebGLRenderingContext.prototype.getSupportedExtensions;"
      "WebGLRenderingContext.prototype.getSupportedExtensions=function(){"
      "var e=_origGetExt.call(this)||[];"
      "return e.filter(function(x){return x.indexOf('debug')===-1;});"
      "};",
      vendor.c_str(), renderer.c_str());
}

std::string AntiDetectionEngine::GenerateAudioProtector(int seed) const {
  return base::StringPrintf(
      // AnalyserNode.getFloatFrequencyData: add deterministic noise
      "var _origGetFloat=AnalyserNode.prototype.getFloatFrequencyData;"
      "AnalyserNode.prototype.getFloatFrequencyData=function(arr){"
      "_origGetFloat.call(this,arr);"
      "for(var i=0;i<arr.length;i++){"
      "var n=((_pcNoise(0,%d+i)-128)/256)*0.5;"
      "arr[i]+=n;"
      "}"
      "};"
      // AnalyserNode.getByteFrequencyData: add deterministic noise
      "var _origGetByte=AnalyserNode.prototype.getByteFrequencyData;"
      "AnalyserNode.prototype.getByteFrequencyData=function(arr){"
      "_origGetByte.call(this,arr);"
      "for(var i=0;i<arr.length;i++){"
      "arr[i]=_pcNoise(arr[i],%d+i);"
      "}"
      "};"
      // AudioBuffer.getChannelData: add deterministic noise
      "var _origGetChannel=AudioBuffer.prototype.getChannelData;"
      "AudioBuffer.prototype.getChannelData=function(ch){"
      "var d=_origGetChannel.call(this,ch);"
      "for(var i=0;i<d.length;i++){"
      "d[i]+=((_pcNoise(128,%d+i)-128)/256)*0.0001;"
      "}"
      "return d;"
      "};",
      seed, seed, seed);
}

std::string AntiDetectionEngine::GenerateFontProtector(int seed) const {
  return base::StringPrintf(
      // Override queryLocalFonts if available (Font Local Font Access API)
      "if('queryLocalFonts' in window){"
      "var _origQueryFonts=window.queryLocalFonts;"
      "window.queryLocalFonts=function(){"
      "return _origQueryFonts.apply(this,arguments).then(function(fonts){"
      "var allowed=['Arial','Helvetica','Times New Roman','Courier New',"
      "'Georgia','Palatino','Garamond','Bookman','Comic Sans MS',"
      "'Trebuchet MS','Arial Black','Impact','Verdana','Tahoma',"
      "'Segoe UI','Calibri','Cambria','Consolas','DejaVu Sans',"
      "'DejaVu Serif','DejaVu Sans Mono','Liberation Sans','Liberation Serif',"
      "'Liberation Mono','Noto Sans','Noto Serif','Roboto','Open Sans',"
      "'Source Sans Pro','Ubuntu','Cantarell','Menlo','Monaco'];"
      "return fonts.filter(function(f){"
      "return allowed.some(function(a){"
      "return f.family.toLowerCase().indexOf(a.toLowerCase())>=0;"
      "});"
      "});"
      "});"
      "};"
      "}"
      // Fuzz font measurement offsets deterministically
      "var _origMeasureText=CanvasRenderingContext2D.prototype.measureText;"
      "CanvasRenderingContext2D.prototype.measureText=function(text){"
      "var m=_origMeasureText.call(this,text);"
      "var orig={"
      "width:m.width,"
      "actualBoundingBoxLeft:m.actualBoundingBoxLeft||0,"
      "actualBoundingBoxRight:m.actualBoundingBoxRight||0,"
      "actualBoundingBoxAscent:m.actualBoundingBoxAscent||0,"
      "actualBoundingBoxDescent:m.actualBoundingBoxDescent||0"
      "};"
      // Apply deterministic micro-adjustments
      "m.width=orig.width+((_pcNoise(128,%d)-128)/256)*0.1;"
      "m.actualBoundingBoxLeft=orig.actualBoundingBoxLeft+((_pcNoise(128,%d+1)-128)/256)*0.1;"
      "m.actualBoundingBoxRight=orig.actualBoundingBoxRight+((_pcNoise(128,%d+2)-128)/256)*0.1;"
      "return m;"
      "};",
      seed, seed, seed);
}

std::string AntiDetectionEngine::GenerateWebRTCProtector() const {
  return base::StringPrintf(
      // Wrap RTCPeerConnection to prevent IP leakage
      "var _origRTC=window.RTCPeerConnection;"
      "if(_origRTC){"
      "var _RTCWrapper=function(config,constraints){"
      "if(config&&config.iceServers){"
      "config.iceServers=config.iceServers.map(function(s){"
      "return{urls:s.urls,username:s.username,credential:s.credential};"
      "});"
      "}"
      "var pc=new _origRTC(config,constraints);"
      "var _origAddIceCandidate=pc.addIceCandidate;"
      "pc.addIceCandidate=function(candidate){"
      "if(candidate&&candidate.candidate){"
      "var c=candidate.candidate;"
      // Filter out srflx (server reflexive) candidates that reveal real IP
      "if(c.indexOf('srflx')!==-1){"
      "return Promise.resolve();"
      "}"
      "}"
      "return _origAddIceCandidate.apply(this,arguments);"
      "};"
      "return pc;"
      "};"
      "_RTCWrapper.prototype=_origRTC.prototype;"
      "window.RTCPeerConnection=_RTCWrapper;"
      "if(typeof window.webkitRTCPeerConnection!=='undefined'){"
      "window.webkitRTCPeerConnection=_RTCWrapper;"
      "}"
      "}"
      // Also override newer unified API
      "if(typeof RTCPeerConnection!=='undefined'){"
      "Object.defineProperty(navigator,'mediaDevices',{"
      "get:function(){"
      "var d=Object.getOwnPropertyDescriptor(Navigator.prototype,'mediaDevices')"
      "  ||Object.getOwnPropertyDescriptor(navigator,'mediaDevices');"
      "if(d&&d.get){"
      "var origDevices=d.get.call(navigator);"
      "var wrapper=Object.create(origDevices);"
      "wrapper.enumerateDevices=function(){"
      "return origDevices.enumerateDevices().then(function(devices){"
      "return devices.map(function(d){"
      "return{kind:d.kind,deviceId:'',groupId:'',label:d.kind==='videoinput'?'camera':'audio'};"
      "});"
      "});"
      "};"
      "return wrapper;"
      "}"
      "return undefined;"
      "}"
      "});"
      "}");
}

std::string AntiDetectionEngine::GenerateWebdriverRemover() const {
  return base::StringPrintf(
      // Remove navigator.webdriver flag
      "try{"
      "Object.defineProperty(navigator,'webdriver',"
      "{get:function(){return undefined;},configurable:true});"
      "}catch(e){}"
      // Remove other automation signatures
      "try{delete window.cdc_adoQpoasnfa76pfcZLmcfl_Array;"
      "delete window.cdc_adoQpoasnfa76pfcZLmcfl_Promise;"
      "delete window.cdc_adoQpoasnfa76pfcfl_Symbol;"
      "}catch(e){}"
      // Override plugins to look natural
      "try{"
      "Object.defineProperty(navigator,'plugins',"
      "{get:function(){"
      "return[{name:'PDF Viewer',filename:'internal-pdf-viewer',"
      "description:'Portable Document Format'},"
      "{name:'Chrome PDF Viewer',filename:'internal-pdf-viewer',"
      "description:'Portable Document Format'},"
      "{name:'Chromium PDF Viewer',filename:'internal-pdf-viewer',"
      "description:'Portable Document Format'},"
      "{name:'Microsoft Edge PDF Viewer',filename:'internal-pdf-viewer',"
      "description:'Portable Document Format'},"
      "{name:'WebKit built-in PDF',filename:'internal-pdf-viewer',"
      "description:'Portable Document Format'}];"
      "},configurable:true});"
      "}catch(e){}"
      // Override languages for consistency
      "try{"
      "Object.defineProperty(navigator,'languages',"
      "{get:function(){return['en-US','en'];},configurable:true});"
      "}catch(e){}");
}

}  // namespace purecloak
