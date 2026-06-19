// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/views/side_panel/purecloak_side_panel_coordinator.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "purecloak/browser/profiles/store_provider.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.h"

namespace purecloak {

void CreateAndRegisterPureCloakSidePanel(BrowserWindowInterface* browser,
                                         SidePanelRegistry* global_registry) {
  if (!browser) {
    return;
  }

  auto* profile = browser->GetProfile();
  if (!profile) {
    return;
  }

  auto* ws_store = StoreProvider::GetInstance().GetWorkspaceStore(profile);

  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kPureCloakWorkspace),
      base::BindRepeating(
          [](WorkspaceStore* ws,
             SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
            return std::make_unique<PureCloakSidePanelView>(ws);
          },
          base::Unretained(ws_store)),
      base::RepeatingCallback<int()>()));
}

}  // namespace purecloak
