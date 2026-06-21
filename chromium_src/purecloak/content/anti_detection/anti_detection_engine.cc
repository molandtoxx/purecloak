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

  // Phase 0: P0 anti-detection gap fixes (guarded by explicit config).
  if (workspace.hardware_concurrency > 0) {
    script += GenerateHardwareConcurrencyProtector(
        workspace.hardware_concurrency);
    script += "\n";
  }
  if (workspace.screen_width > 0 && workspace.screen_height > 0) {
    script += GenerateScreenProtector(workspace.screen_width,
                                       workspace.screen_height);
    script += "\n";
  }
  if (!workspace.timezone.empty()) {
    script += GenerateTimezoneProtector(workspace.timezone);
    script += "\n";
  }
  if (!workspace.platform.empty()) {
    script += GeneratePlatformProtector(workspace.platform);
    script += "\n";
  }
  if (!workspace.color_scheme.empty()) {
    script += GenerateColorSchemeProtector(workspace.color_scheme);
    script += "\n";
  }

  // Phase 1: Deep anti-detection reinforcement.
  if (!workspace.platform.empty()) {
    script += GenerateFeatureConsistencyProtector(workspace.platform);
    script += "\n";
  }
  if (!workspace.gpu_vendor.empty()) {
    script += GenerateWebGLExtensionFilter(workspace.gpu_vendor);
    script += "\n";
  }
  script += GeneratePerformanceProtector();
  script += "\n";
  // Derive plausible lat/lng from configured timezone for geolocation spoofing.
  if (!workspace.timezone.empty()) {
    double lat = 40.7128, lng = -74.0060;
    if (workspace.timezone.find("Europe") != std::string::npos) {
      lat = 48.8566; lng = 2.3522;
    } else if (workspace.timezone.find("Asia/Shanghai") != std::string::npos ||
               workspace.timezone.find("Asia/Hong_Kong") != std::string::npos) {
      lat = 31.2304; lng = 121.4737;
    } else if (workspace.timezone.find("Asia/Tokyo") != std::string::npos) {
      lat = 35.6762; lng = 139.6503;
    } else if (workspace.timezone.find("Australia") != std::string::npos) {
      lat = -33.8688; lng = 151.2093;
    } else if (workspace.timezone.find("America/Los_Angeles") != std::string::npos) {
      lat = 34.0522; lng = -118.2437;
    }
    script += GenerateGeolocationProtector(lat, lng);
    script += "\n";
  }
  script += GenerateBatteryProtector();
  script += "\n";
  script += GenerateNetworkInfoProtector();
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

// ── Phase 1: Deep anti-detection reinforcement ────────────────────────────

std::string AntiDetectionEngine::GenerateFeatureConsistencyProtector(
    const std::string& platform) const {
  if (platform.empty())
    return "";

  // Build platform-specific API blacklist. These are APIs that should NOT
  // exist on the claimed platform — hiding them prevents contradictory
  // fingerprint signals.
  std::string apis;
  auto add_api = [&](const std::string& name) {
    if (!apis.empty()) apis += ",";
    apis += "'" + name + "'";
  };

  if (platform == "windows" || platform == "macos" || platform == "linux") {
    // Desktop platforms should not expose mobile-only APIs.
    add_api("mediaSession");
    add_api("virtualKeyboard");
    add_api("wakeLock");
  }
  if (platform == "linux") {
    // Linux Chrome does not support bluetooth or serial by default.
    add_api("bluetooth");
    add_api("serial");
  }
  if (platform == "macos") {
    // macOS Chrome does not support serial.
    add_api("serial");
  }

  if (apis.empty())
    return "";

  return base::StringPrintf(
      "try{"
      "var _hidden=[%s];"
      "for(var _i=0;_i<_hidden.length;_i++){"
      "var _api=_hidden[_i];"
      "try{"
      "if(_api in navigator){"
      "Object.defineProperty(navigator,_api,{value:undefined,"
      "writable:false,configurable:true});"
      "}"
      "}catch(e){}"
      "try{"
      "if(_api in window){"
      "Object.defineProperty(window,_api,{value:undefined,"
      "writable:false,configurable:true});"
      "}"
      "}catch(e){}"
      "}"
      "}catch(e){}",
      apis.c_str());
}

std::string AntiDetectionEngine::GenerateWebGLExtensionFilter(
    const std::string& gpu_vendor) const {
  // Build a set of extensions to block based on GPU vendor.
  // In addition to filtering 'debug' extensions, also filter GPU-specific
  // extensions that could identify hardware despite vendor/renderer spoofing.
  //
  // NVIDIA-specific: GL_NV_*, GLX_NV_*, WGL_NV_*
  // AMD-specific: GL_AMD_*, GL_ATI_*
  // Intel-specific: GL_INTEL_*
  std::string blocked_patterns = "'NV_','AMD_','ATI_','INTEL_','QCOM_'";
  if (gpu_vendor.find("NVIDIA") != std::string::npos ||
      gpu_vendor.find("AMD") != std::string::npos) {
    // If we're spoofing as a specific vendor, still block others but
    // keep the spoofed vendor's extensions for consistency.
    blocked_patterns = "'NV_','AMD_','ATI_','INTEL_','QCOM_','SAMSUNG_','IMG_'";
  }

  return base::StringPrintf(
      "try{"
      "var _blocked=[%s,'debug'];"
      "var _origExt=WebGLRenderingContext.prototype.getSupportedExtensions;"
      "WebGLRenderingContext.prototype.getSupportedExtensions=function(){"
      "var e=_origExt.call(this)||[];"
      "return e.filter(function(x){"
      "for(var _b=0;_b<_blocked.length;_b++){"
      "if(x.indexOf(_blocked[_b])!==-1)return false;"
      "}"
      "return true;"
      "});"
      "};"
      "if(typeof WebGL2RenderingContext!=='undefined'){"
      "var _origExt2=WebGL2RenderingContext.prototype.getSupportedExtensions;"
      "WebGL2RenderingContext.prototype.getSupportedExtensions=function(){"
      "var e=_origExt2.call(this)||[];"
      "return e.filter(function(x){"
      "for(var _b=0;_b<_blocked.length;_b++){"
      "if(x.indexOf(_blocked[_b])!==-1)return false;"
      "}"
      "return true;"
      "});"
      "};"
      "}"
      "}catch(e){}",
      blocked_patterns.c_str());
}

std::string AntiDetectionEngine::GeneratePerformanceProtector() const {
  return std::string(
      "try{"
      // Round performance.now() to reduce precision (100μs).
      "var _origNow=performance.now.bind(performance);"
      "performance.now=function(){"
      "return Math.floor(_origNow()*10)/10;"
      "};"
      // Hide performance.memory if present.
      "if(performance.memory){"
      "Object.defineProperty(performance,'memory',{value:undefined,"
      "writable:false,configurable:true});"
      "}"
      "}catch(e){}");
}

std::string AntiDetectionEngine::GenerateGeolocationProtector(
    double lat, double lng) const {
  if (lat == 0.0 && lng == 0.0)
    return "";  // No valid coordinates.
  return base::StringPrintf(
      "try{"
      "var _pos={coords:{latitude:%f,longitude:%f,"
      "accuracy:100,altitude:null,altitudeAccuracy:null,"
      "heading:null,speed:null},timestamp:Date.now()};"
      "var _origGet=navigator.geolocation.getCurrentPosition;"
      "navigator.geolocation.getCurrentPosition=function(success,error,opts){"
      "setTimeout(function(){success(_pos);},50);"
      "};"
      "navigator.geolocation.watchPosition=function(success,error,opts){"
      "setTimeout(function(){success(_pos);},50);"
      "var _id=Math.floor(Math.random()*99999)+1;"
      "return _id;"
      "};"
      "}catch(e){}",
      lat, lng);
}

std::string AntiDetectionEngine::GenerateBatteryProtector() const {
  return std::string(
      "try{"
      "if(navigator.getBattery){"
      "var _origGetBattery=navigator.getBattery.bind(navigator);"
      "navigator.getBattery=function(){"
      "return _origGetBattery().then(function(battery){"
      "Object.defineProperties(battery,{"
      "charging:{get:function(){return true;},configurable:true},"
      "chargingTime:{get:function(){return 0;},configurable:true},"
      "dischargingTime:{get:function(){return Infinity;},configurable:true},"
      "level:{get:function(){return 1.0;},configurable:true}"
      "});"
      "return battery;"
      "});"
      "};"
      "}"
      "}catch(e){}");
}

std::string AntiDetectionEngine::GenerateNetworkInfoProtector() const {
  return std::string(
      "try{"
      "if(navigator.connection){"
      "Object.defineProperties(navigator.connection,{"
      "effectiveType:{get:function(){return '4g';},configurable:true},"
      "downlink:{get:function(){return 10;},configurable:true},"
      "rtt:{get:function(){return 50;},configurable:true},"
      "saveData:{get:function(){return false;},configurable:true}"
      "});"
      "}"
      "}catch(e){}");
}

// ── Phase 0: P0 anti-detection gap fixes ──────────────────────────────────

std::string AntiDetectionEngine::GenerateHardwareConcurrencyProtector(
    int hw_concurrency) const {
  if (hw_concurrency <= 0)
    return "";
  return base::StringPrintf(
      "try{"
      "Object.defineProperty(navigator,'hardwareConcurrency',"
      "{get:function(){return %d;},configurable:true});"
      "}catch(e){}",
      hw_concurrency);
}

std::string AntiDetectionEngine::GenerateDeviceMemoryProtector(
    int device_memory) const {
  if (device_memory <= 0)
    return "";
  return base::StringPrintf(
      "try{"
      "Object.defineProperty(navigator,'deviceMemory',"
      "{get:function(){return %d;},configurable:true});"
      "}catch(e){}",
      device_memory);
}

std::string AntiDetectionEngine::GenerateScreenProtector(
    int width, int height) const {
  if (width <= 0 || height <= 0)
    return "";
  return base::StringPrintf(
      "try{"
      "var _w=%d,_h=%d;"
      "Object.defineProperties(screen,{"
      "width:{get:function(){return _w;},configurable:true},"
      "height:{get:function(){return _h;},configurable:true},"
      "availWidth:{get:function(){return _w;},configurable:true},"
      "availHeight:{get:function(){return _h;},configurable:true},"
      "colorDepth:{get:function(){return 24;},configurable:true},"
      "pixelDepth:{get:function(){return 24;},configurable:true}"
      "});"
      "}catch(e){}",
      width, height);
}

std::string AntiDetectionEngine::GenerateTimezoneProtector(
    const std::string& tz) const {
  if (tz.empty())
    return "";
  // Escape single quotes in tz for safe embedding in JS string.
  std::string escaped = tz;
  for (size_t i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '\'') {
      escaped.insert(i, "\\");
      ++i;
    }
  }
  return base::StringPrintf(
      "try{"
      // Override Intl.DateTimeFormat timeZone resolution.
      "var _tz='%s';"
      "var _origDTF=Intl.DateTimeFormat;"
      "Intl.DateTimeFormat=function(locales,opts){"
      "opts=Object.assign({},opts);"
      "if(opts.timeZone)opts.timeZone=_tz;"
      "return new _origDTF(locales,opts);"
      "};"
      "Intl.DateTimeFormat.prototype=_origDTF.prototype;"
      "Intl.DateTimeFormat.resolvedOptions=function(){"
      "var r=_origDTF().resolvedOptions();"
      "r.timeZone=_tz;"
      "return r;"
      "};"
      // Override Date.getTimezoneOffset — return offset consistent with _tz.
      // Simplified: map common tz to offset minutes.
      "var _tzOffset=(function(){"
      "var map={'America/New_York':300,'America/Chicago':360,"
      "'America/Denver':420,'America/Los_Angeles':480,"
      "'America/Anchorage':540,'Pacific/Honolulu':600,"
      "'Europe/London':0,'Europe/Paris':-60,'Europe/Berlin':-60,"
      "'Europe/Moscow':-180,'Europe/Dublin':0,"
      "'Asia/Shanghai':-480,'Asia/Tokyo':-540,'Asia/Seoul':-540,"
      "'Asia/Hong_Kong':-480,'Asia/Singapore':-480,'Asia/Kolkata':-330,"
      "'Asia/Dubai':-240,'Asia/Jakarta':-420,"
      "'Australia/Sydney':-660,'Australia/Melbourne':-660,"
      "'Pacific/Auckland':-720,"
      "'Canada/Eastern':300,'Canada/Central':360,"
      "'Brazil/East':-180,"
      "'UTC':0,'GMT':0};"
      "return map[_tz];"
      "})();"
      "if(_tzOffset!==undefined){"
      "var _origGetTO=Date.prototype.getTimezoneOffset;"
      "Date.prototype.getTimezoneOffset=function(){return _tzOffset;};"
      "}"
      "}catch(e){}",
      escaped.c_str());
}

std::string AntiDetectionEngine::GeneratePlatformProtector(
    const std::string& platform) const {
  if (platform.empty())
    return "";
  std::string js_platform;
  if (platform == "windows")
    js_platform = "Win32";
  else if (platform == "macos")
    js_platform = "MacIntel";
  else if (platform == "linux")
    js_platform = "Linux x86_64";
  else
    js_platform = platform;
  return base::StringPrintf(
      "try{"
      "Object.defineProperty(navigator,'platform',"
      "{get:function(){return '%s';},configurable:true});"
      "}catch(e){}",
      js_platform.c_str());
}

std::string AntiDetectionEngine::GenerateColorSchemeProtector(
    const std::string& scheme) const {
  if (scheme.empty())
    return "";
  bool is_dark = (scheme == "dark");
  bool is_light = (scheme == "light");
  return base::StringPrintf(
      "try{"
      "var _matchMedia=window.matchMedia;"
      "window.matchMedia=function(query){"
      "var m=_matchMedia.call(window,query);"
      "if(query.indexOf('prefers-color-scheme')!==-1){"
      "var _origGet=m.__lookupGetter__('matches');"
      "Object.defineProperty(m,'matches',{get:function(){"
      "return query.indexOf('dark')!==-1?%s:"
      "query.indexOf('light')!==-1?%s:"
      "false;"
      "},configurable:true});"
      "}"
      "return m;"
      "};"
      "}catch(e){}",
      is_dark ? "true" : "false",
      is_light ? "true" : "false");
}

}  // namespace purecloak
