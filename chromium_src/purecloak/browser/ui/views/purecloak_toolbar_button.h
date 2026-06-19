// Copyright 2026 PureCloak Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_VIEWS_PURECLOAK_TOOLBAR_BUTTON_H_
#define PURECLOAK_BROWSER_UI_VIEWS_PURECLOAK_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;

namespace purecloak {

class PureCloakToolbarButton : public ToolbarButton {
  METADATA_HEADER(PureCloakToolbarButton, ToolbarButton)

 public:
  explicit PureCloakToolbarButton(Browser* browser);
  ~PureCloakToolbarButton() override;

 private:
  void ButtonPressed();
  const raw_ptr<Browser> browser_;
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_VIEWS_PURECLOAK_TOOLBAR_BUTTON_H_
