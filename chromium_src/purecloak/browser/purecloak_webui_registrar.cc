// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/purecloak_webui_registrar.h"

#include "base/logging.h"
#include "content/public/browser/webui_config_map.h"
#include "purecloak/browser/ui/webui/purecloak_ui.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace purecloak {

void RegisterPureCloakWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<PureCloakUIConfig>());
}
}  // namespace purecloak
