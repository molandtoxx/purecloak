// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_PURECLOAK_WEBUI_REGISTRAR_H_
#define PURECLOAK_BROWSER_PURECLOAK_WEBUI_REGISTRAR_H_

namespace purecloak {

// Registers PureCloak's WebUIConfigs with the global WebUIConfigMap.
// Called from PureCloakBrowserMainParts during startup, after
// RegisterChromeWebUIConfigs().
void RegisterPureCloakWebUIConfigs();

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_PURECLOAK_WEBUI_REGISTRAR_H_
