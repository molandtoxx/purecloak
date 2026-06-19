// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_UI_H_
#define PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace purecloak {

class PureCloakUI;

// WebUIConfig for //purecloak/
class PureCloakUIConfig : public content::DefaultWebUIConfig<PureCloakUI> {
 public:
  PureCloakUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, "purecloak") {}
};

// WebUIController for //purecloak/
class PureCloakUI : public content::WebUIController {
 public:
  explicit PureCloakUI(content::WebUI* web_ui);
  ~PureCloakUI() override;
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_UI_H_
