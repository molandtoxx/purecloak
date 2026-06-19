// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_WORKSPACE_MANAGEMENT_DIALOGS_H_
#define PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_WORKSPACE_MANAGEMENT_DIALOGS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Combobox;
class Textfield;
class View;
}  // namespace views

namespace purecloak {

// Dialog for creating a new workspace.
class CreateWorkspaceDialog : public views::DialogDelegate {
 public:
  explicit CreateWorkspaceDialog(bool enable_fingerprint_type = true);

  std::string GetWorkspaceName() const;
  int GetWorkspaceTypeIndex() const;

 private:
  std::unique_ptr<views::View> CreateContentsView(bool enable_fingerprint_type);

  raw_ptr<views::Textfield> name_field_ = nullptr;
  raw_ptr<views::Combobox> type_combobox_ = nullptr;
};

// Dialog for renaming a workspace.
class RenameWorkspaceDialog : public views::DialogDelegate {
 public:
  explicit RenameWorkspaceDialog(const std::string& current_name);

  std::string GetNewName() const;

 private:
  std::unique_ptr<views::View> CreateContentsView(const std::string& name);

  raw_ptr<views::Textfield> name_field_ = nullptr;
};

// Dialog for confirming workspace deletion.
class DeleteWorkspaceDialog : public views::DialogDelegate {
 public:
  explicit DeleteWorkspaceDialog(const std::string& workspace_name);

 private:
  std::unique_ptr<views::View> CreateContentsView(const std::string& workspace_name);
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_WORKSPACE_MANAGEMENT_DIALOGS_H_
