// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/views/purecloak_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "purecloak/common/purecloak_i18n.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button_controller.h"

namespace purecloak {

PureCloakToolbarButton::PureCloakToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&PureCloakToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser) {
  SetTooltipText(purecloak::strings::kToolbarTooltip());
  SetVectorIcon(kSyncIcon);
}

PureCloakToolbarButton::~PureCloakToolbarButton() = default;

void PureCloakToolbarButton::ButtonPressed() {
  // Toggle the PureCloak workspace side panel.
  SidePanelUI::From(browser_.get())
      ->Toggle(SidePanelEntryKey(SidePanelEntryId::kPureCloakWorkspace),
               SidePanelOpenTrigger::kToolbarButton);
}

BEGIN_METADATA(PureCloakToolbarButton)
END_METADATA

}  // namespace purecloak
