// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/webui/purecloak_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "purecloak/browser/ui/webui/purecloak_handler.h"
#include "purecloak/resources/grit/purecloak_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace purecloak {

PureCloakUI::PureCloakUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  auto* profile = ::Profile::FromWebUI(web_ui);
  auto* source = content::WebUIDataSource::CreateAndAdd(profile, "purecloak");

  source->SetDefaultResource(IDR_PURECLOAK_PURECLOAK_HTML);
  source->UseStringsJs();

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'unsafe-inline';");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types default;");

  web_ui->AddMessageHandler(std::make_unique<PureCloakHandler>(profile));
}

PureCloakUI::~PureCloakUI() = default;

}  // namespace purecloak
