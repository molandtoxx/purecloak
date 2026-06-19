// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/views/side_panel/workspace_management_dialogs.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "purecloak/common/purecloak_i18n.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace purecloak {

namespace {

// Combobox model for workspace type selection.
class WorkspaceTypeModel : public ui::ComboboxModel {
 public:
  WorkspaceTypeModel() = default;
  ~WorkspaceTypeModel() override = default;

  size_t GetItemCount() const override { return 2; }
  std::u16string GetItemAt(size_t index) const override {
    switch (index) {
      case 0:
        return purecloak::strings::kNormal();
      case 1:
        return purecloak::strings::kFingerprint();
      default:
        return u"";
    }
  }
};

// Helper to add a textfield + label row to a dialog.
views::View* CreateLabeledTextfieldRow(views::View* parent,
                                       const std::u16string& label_text,
                                       const std::u16string& default_text,
                                       raw_ptr<views::Textfield>* out_field) {
  auto* field = new views::Textfield();
  field->SetAccessibleName(label_text);
  field->SetText(default_text);
  field->SelectAll(true);
  *out_field = field;
  parent->AddChildView(field);
  return field;
}

// Helper to create a label.
views::Label* CreateLabel(views::View* parent,
                          const std::u16string& text) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>(text));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

}  // namespace

// --- CreateWorkspaceDialog ---

CreateWorkspaceDialog::CreateWorkspaceDialog(bool enable_fingerprint_type) {
  SetTitle(purecloak::strings::kCreateWorkspace());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, purecloak::strings::kCreate());
  SetButtonLabel(ui::mojom::DialogButton::kCancel, purecloak::strings::kCancel());
  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(320);
  SetContentsView(CreateContentsView(enable_fingerprint_type));
}

std::unique_ptr<views::View> CreateWorkspaceDialog::CreateContentsView(
    bool enable_fingerprint_type) {
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(20), 12));

  CreateLabel(contents.get(), purecloak::strings::kName());
  CreateLabeledTextfieldRow(contents.get(), purecloak::strings::kWorkspaceName(), u"", &name_field_);

  CreateLabel(contents.get(), purecloak::strings::kType());
  type_combobox_ =
      contents->AddChildView(std::make_unique<views::Combobox>(
          std::make_unique<WorkspaceTypeModel>()));
  type_combobox_->SetAccessibleName(purecloak::strings::kWorkspaceType());
  type_combobox_->SetEnabled(enable_fingerprint_type);

  return contents;
}

std::string CreateWorkspaceDialog::GetWorkspaceName() const {
  return base::UTF16ToUTF8(name_field_->GetText());
}

int CreateWorkspaceDialog::GetWorkspaceTypeIndex() const {
  return type_combobox_->GetSelectedIndex().value_or(0);
}

// --- RenameWorkspaceDialog ---

RenameWorkspaceDialog::RenameWorkspaceDialog(const std::string& current_name) {
  SetTitle(purecloak::strings::kRenameWorkspace());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, purecloak::strings::kRename());
  SetButtonLabel(ui::mojom::DialogButton::kCancel, purecloak::strings::kCancel());
  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(320);
  SetContentsView(CreateContentsView(current_name));
}

std::unique_ptr<views::View> RenameWorkspaceDialog::CreateContentsView(
    const std::string& current_name) {
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(20), 12));

  CreateLabel(contents.get(), purecloak::strings::kNewName());
  CreateLabeledTextfieldRow(contents.get(), purecloak::strings::kWorkspaceName(),
                            base::UTF8ToUTF16(current_name), &name_field_);
  return contents;
}

std::string RenameWorkspaceDialog::GetNewName() const {
  return base::UTF16ToUTF8(name_field_->GetText());
}

// --- DeleteWorkspaceDialog ---

DeleteWorkspaceDialog::DeleteWorkspaceDialog(
    const std::string& workspace_name) {
  SetTitle(purecloak::strings::kDeleteWorkspace());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, purecloak::strings::kDelete());
  SetButtonLabel(ui::mojom::DialogButton::kCancel, purecloak::strings::kCancel());
  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(320);
  SetContentsView(CreateContentsView(workspace_name));
}

std::unique_ptr<views::View> DeleteWorkspaceDialog::CreateContentsView(
    const std::string& workspace_name) {
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(20), 8));

  auto* label = CreateLabel(
      contents.get(), purecloak::strings::kDeleteWorkspaceConfirm(workspace_name));
  label->SetMultiLine(true);

  return contents;
}

}  // namespace purecloak
