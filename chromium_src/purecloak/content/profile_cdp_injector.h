// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_CONTENT_PROFILE_CDP_INJECTOR_H_
#define PURECLOAK_CONTENT_PROFILE_CDP_INJECTOR_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

// Generates and applies CDP (Chrome DevTools Protocol) commands to inject
  // workspace settings into a running workspace subprocess.
//
// The injector operates in two modes:
// 1. Command Generation: Produces CDP command payloads for batch execution
// 2. Script Generation: Produces JS injection scripts for
//    Page.addScriptToEvaluateOnNewDocument
//
// Phase 3 extends this with anti-detection scripts (Canvas, WebGL, Audio).
class ProfileCDPInjector {
 public:
  ProfileCDPInjector();
  ~ProfileCDPInjector();

  // Sets the CDP endpoint URL (e.g. "http://127.0.0.1:9333").
  void SetCDPEndpoint(const std::string& cdp_url);

  // Generates all CDP commands for a workspace as a list of JSON-RPC payloads.
  // Each entry is a {method, params} dictionary.
  std::vector<base::DictValue> GenerateCDPCommands(
      const Workspace& workspace) const;

  // Generates the navigator override JavaScript for
  // Page.addScriptToEvaluateOnNewDocument.
  std::string GenerateNavigatorOverrides(const Workspace& workspace) const;

  // Generates the screen dimension override JavaScript.
  std::string GenerateScreenOverrides(const Workspace& workspace) const;

  // Generates the complete injection script combining navigator overrides,
  // screen overrides, and (in Phase 3) anti-detection scripts.
  std::string GenerateInjectionScript(const Workspace& workspace,
                                       bool include_anti_detection = false) const;

  // Generates a single CDP command payload.
  static base::DictValue MakeCommand(const std::string& method,
                                       base::DictValue params);

  // Generates the Emulation.setDeviceMetricsOverride command.
  static base::DictValue MakeScreenOverride(int width, int height);

  // Generates the Emulation.setTimezoneOverride command.
  static base::DictValue MakeTimezoneOverride(const std::string& timezone);

  // Generates the Emulation.setAutoDarkModeOverride command.
  static base::DictValue MakeDarkModeOverride(bool enabled);

  // Generates the Page.addScriptToEvaluateOnNewDocument command.
  static base::DictValue MakeScriptInjection(const std::string& source);

 private:
  std::string cdp_url_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace purecloak

#endif  // PURECLOAK_CONTENT_PROFILE_CDP_INJECTOR_H_
