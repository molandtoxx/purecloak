// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/profile_cdp_injector.h"

#include <utility>

#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "purecloak/content/anti_detection/anti_detection_engine.h"
#include "purecloak/content/anti_detection/humanize_engine.h"

namespace purecloak {

ProfileCDPInjector::ProfileCDPInjector() = default;

ProfileCDPInjector::~ProfileCDPInjector() = default;

void ProfileCDPInjector::SetCDPEndpoint(const std::string& cdp_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdp_url_ = cdp_url;
}

// static
base::DictValue ProfileCDPInjector::MakeCommand(const std::string& method,
                                                  base::DictValue params) {
  base::DictValue command;
  command.Set("method", method);
  command.Set("params", std::move(params));
  return command;
}

// static
base::DictValue ProfileCDPInjector::MakeScreenOverride(int width,
                                                        int height) {
  base::DictValue params;
  params.Set("width", width);
  params.Set("height", height);
  params.Set("deviceScaleFactor", 1);
  params.Set("mobile", false);
  return MakeCommand("Emulation.setDeviceMetricsOverride", std::move(params));
}

// static
base::DictValue ProfileCDPInjector::MakeTimezoneOverride(
    const std::string& timezone) {
  base::DictValue params;
  params.Set("timezoneId", timezone);
  return MakeCommand("Emulation.setTimezoneOverride", std::move(params));
}

// static
base::DictValue ProfileCDPInjector::MakeDarkModeOverride(bool enabled) {
  base::DictValue params;
  params.Set("enabled", enabled);
  return MakeCommand("Emulation.setAutoDarkModeOverride", std::move(params));
}

// static
base::DictValue ProfileCDPInjector::MakeScriptInjection(
    const std::string& source) {
  base::DictValue params;
  params.Set("source", source);
  return MakeCommand("Page.addScriptToEvaluateOnNewDocument",
                     std::move(params));
}

std::vector<base::DictValue> ProfileCDPInjector::GenerateCDPCommands(
    const Workspace& workspace) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<base::DictValue> commands;

  if (workspace.screen_width > 0 && workspace.screen_height > 0) {
    commands.push_back(
        MakeScreenOverride(workspace.screen_width, workspace.screen_height));
  }

  if (!workspace.timezone.empty()) {
    commands.push_back(MakeTimezoneOverride(workspace.timezone));
  }

  if (workspace.color_scheme == "dark") {
    commands.push_back(MakeDarkModeOverride(true));
  } else if (workspace.color_scheme == "light") {
    commands.push_back(MakeDarkModeOverride(false));
  }

  std::string injection_script = GenerateInjectionScript(workspace, true);
  if (!injection_script.empty()) {
    commands.push_back(MakeScriptInjection(injection_script));
  }

  return commands;
}

std::string ProfileCDPInjector::GenerateNavigatorOverrides(
    const Workspace& workspace) const {
  std::string platform = workspace.platform;
  if (platform == "windows") platform = "Win32";
  else if (platform == "macos") platform = "MacIntel";
  else if (platform == "linux") platform = "Linux x86_64";
  else if (platform.empty()) platform = "Win32";

  int concurrency = workspace.hardware_concurrency;
  if (concurrency <= 0) {
    concurrency = 4;
  }

  // Escape user-agent string for safe JS embedding.
  std::string escaped_ua;
  if (!workspace.user_agent.empty()) {
    base::EscapeJSONString(workspace.user_agent, false, &escaped_ua);
    // Also escape single quotes since the JS template uses single-quoted
    // string literals.
    for (size_t i = 0; i < escaped_ua.size(); ++i) {
      if (escaped_ua[i] == '\'') {
        escaped_ua.insert(i, "\\");
        ++i;
      }
    }
  }

  return base::StringPrintf(
      "(function(){"
      "var overrides={"
      "platform:{get:function(){return '%s';}},"
      "hardwareConcurrency:{get:function(){return %d;}},"
      "%s"
      "};"
      "for(var k in overrides){"
      "try{Object.defineProperty(navigator,k,overrides[k]);}catch(e){}"
      "}"
      "})();",
      platform.c_str(), concurrency,
      escaped_ua.empty() ? "" :
      base::StringPrintf("userAgent:{get:function(){return '%s';}},",
                         escaped_ua.c_str()).c_str());
}

std::string ProfileCDPInjector::GenerateScreenOverrides(
    const Workspace& workspace) const {
  int width = workspace.screen_width > 0 ? workspace.screen_width : 1366;
  int height = workspace.screen_height > 0 ? workspace.screen_height : 768;
  // Window.screen has [Replaceable] in IDL, so we can swap it with a Proxy.
  return base::StringPrintf(
      "(function(){"
      "var _s=window.screen;"
      "if(!_s)return;"
      "try{"
      "window.screen=new Proxy(_s,{"
      "get:function(t,p){"
      "if(p==='width'||p==='availWidth')return %d;"
      "if(p==='height'||p==='availHeight')return %d;"
      "if(p==='availLeft')return 0;"
      "if(p==='availTop')return 0;"
      "var v=t[p];"
      "return typeof v==='function'?v.bind(t):v;"
      "}"
      "});"
      "}catch(e){}"
      "})();",
      width, height);
}

std::string ProfileCDPInjector::GenerateInjectionScript(
    const Workspace& workspace,
    bool include_anti_detection) const {
  std::string script;

  script += GenerateNavigatorOverrides(workspace);

  script += "\n";
  script += GenerateScreenOverrides(workspace);

  if (include_anti_detection) {
    AntiDetectionEngine engine;
    std::string protection_script = engine.GenerateProtectionScript(workspace);
    if (!protection_script.empty()) {
      script += "\n";
      script += protection_script;
    }

    HumanizeEngine humanize;
    std::string humanize_script =
        humanize.GenerateScript(workspace.human_preset);
    if (workspace.humanize && !humanize_script.empty()) {
      script += "\n";
      script += humanize_script;
    }
  }

  return script;
}

}  // namespace purecloak
