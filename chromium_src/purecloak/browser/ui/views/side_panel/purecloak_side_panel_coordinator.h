// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_COORDINATOR_H_
#define PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_COORDINATOR_H_

class BrowserWindowInterface;
class SidePanelRegistry;

namespace purecloak {

// Creates and registers the PureCloak workspace SidePanelEntry in the given
// global (per-window) registry. The content callback creates a placeholder
// PureCloakSidePanelView each time the entry is shown.
void CreateAndRegisterPureCloakSidePanel(BrowserWindowInterface* browser,
                                         SidePanelRegistry* global_registry);

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_COORDINATOR_H_
