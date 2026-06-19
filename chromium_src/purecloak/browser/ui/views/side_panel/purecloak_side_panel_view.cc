// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "purecloak/browser/profiles/workspace.h"
#include "purecloak/browser/ui/views/side_panel/workspace_management_dialogs.h"
#include "purecloak/common/purecloak_i18n.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace purecloak {

namespace {

constexpr int kCardPadding = 12;
constexpr int kCornerRadius = 8;

// Creates a basic text button with a given label and callback.
views::MdTextButton* CreateSmallButton(
    views::View* parent,
    const std::u16string& label,
    base::RepeatingClosure callback) {
  auto* button = parent->AddChildView(
      std::make_unique<views::MdTextButton>(std::move(callback), label));
  button->SetStyle(ui::ButtonStyle::kText);
  return button;
}

}  // namespace

PureCloakSidePanelView::PureCloakSidePanelView(
    WorkspaceStore* workspace_store)
    : workspace_store_(workspace_store) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(0), 0));

  // --- Header ---
  auto* header = AddChildView(std::make_unique<views::View>());
  auto* header_layout = header->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(8), 0));
  auto* title = header->AddChildView(std::make_unique<views::Label>(
      purecloak::strings::kWorkspaces()));
  title->SetFontList(title->font_list().DeriveWithSizeDelta(4));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header_layout->SetFlexForView(title, 1);

  create_workspace_button_ =
      header->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&PureCloakSidePanelView::OnCreateWorkspaceClicked,
                              base::Unretained(this)),
          purecloak::strings::kNewWorkspace()));

  // --- Scrollable workspace list ---
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto* layout_provider = views::LayoutProvider::Get();
  if (layout_provider) {
    scroll_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded));
  }

  container_ = scroll_view_->SetContents(std::make_unique<views::View>());
  container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(8), 4));

  // Observe store.
  workspace_store_->AddObserver(this);

  // Build initial workspace list.
  RebuildWorkspaceList();
}

PureCloakSidePanelView::~PureCloakSidePanelView() {
  workspace_store_->RemoveObserver(this);
}

void PureCloakSidePanelView::OnWorkspacesChanged() {
  RebuildWorkspaceList();
}

void PureCloakSidePanelView::RebuildWorkspaceList() {
  // Clear existing workspace cards (keep header and scroll view).
  container_->RemoveAllChildViews();

  std::vector<Workspace> workspaces = workspace_store_->GetAllWorkspaces();

  if (workspaces.empty()) {
    auto* empty = container_->AddChildView(std::make_unique<views::Label>(
        purecloak::strings::kNoWorkspaces()));
    empty->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty->SetEnabledColor(SK_ColorGRAY);
    empty->SetMultiLine(true);
    return;
  }

  for (const auto& ws : workspaces) {
    // --- Workspace Card ---
    auto* card = container_->AddChildView(std::make_unique<views::View>());
    card->SetBackground(views::CreateRoundedRectBackground(
        SkColorSetARGB(0x0F, 0, 0, 0), kCornerRadius));
    card->SetBorder(views::CreateEmptyBorder(gfx::Insets(kCardPadding)));
    card->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 8));

    // Card header row: type indicator, name, buttons.
    auto* header_row = card->AddChildView(std::make_unique<views::View>());
    auto* header_row_layout = header_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));

    // Type icon label.
    auto* type_label = header_row->AddChildView(
        std::make_unique<views::Label>(
            ws.type == Workspace::Type::kFingerprint
                ? purecloak::strings::kFingerprint()
                : purecloak::strings::kNormal()));
    type_label->SetFontList(type_label->font_list().DeriveWithSizeDelta(-2));

    // Workspace name.
    auto* name_label = header_row->AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(ws.name)));
    name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    name_label->SetFontList(
        name_label->font_list().DeriveWithSizeDelta(1));
    header_row_layout->SetFlexForView(name_label, 1);

    // Rename button.
    CreateSmallButton(
        header_row, u"✏",
        base::BindRepeating(&PureCloakSidePanelView::OnRenameWorkspace,
                            base::Unretained(this), ws.id));

    // Delete button.
    CreateSmallButton(
        header_row, u"🗑",
        base::BindRepeating(&PureCloakSidePanelView::OnDeleteWorkspace,
                            base::Unretained(this), ws.id));
  }
}

void PureCloakSidePanelView::OnCreateWorkspaceClicked() {
  auto dialog = std::make_unique<CreateWorkspaceDialog>(true);
  CreateWorkspaceDialog* dialog_ptr = dialog.get();

  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  dialog_ptr->SetAcceptCallback(base::BindOnce(
      [](PureCloakSidePanelView* view, CreateWorkspaceDialog* dialog) {
        std::string name = dialog->GetWorkspaceName();
        if (name.empty()) {
          return;
        }
        Workspace::Type type = dialog->GetWorkspaceTypeIndex() == 1
                                   ? Workspace::Type::kFingerprint
                                   : Workspace::Type::kNormal;
        view->workspace_store_->CreateWorkspace(
            Workspace::CreateBasic(name, type));
        view->RebuildWorkspaceList();
      },
      this, dialog_ptr));

  views::Widget* dialog_widget = views::Widget::CreateWindowWithParent(
      std::move(dialog), widget->GetNativeWindow());
  dialog_widget->Show();
}

void PureCloakSidePanelView::OnRenameWorkspace(
    const std::string& workspace_id) {
  auto ws = workspace_store_->GetWorkspace(workspace_id);
  if (!ws.has_value()) {
    return;
  }

  auto dialog = std::make_unique<RenameWorkspaceDialog>(ws->name);
  RenameWorkspaceDialog* dialog_ptr = dialog.get();

  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  dialog_ptr->SetAcceptCallback(base::BindOnce(
      [](PureCloakSidePanelView* view, const std::string& ws_id,
         RenameWorkspaceDialog* dialog) {
        std::string new_name = dialog->GetNewName();
        if (!new_name.empty()) {
          view->workspace_store_->UpdateWorkspace(ws_id, new_name);
        }
      },
      this, workspace_id, dialog_ptr));

  views::Widget* dialog_widget = views::Widget::CreateWindowWithParent(
      std::move(dialog), widget->GetNativeWindow());
  dialog_widget->Show();
}

void PureCloakSidePanelView::OnDeleteWorkspace(
    const std::string& workspace_id) {
  auto ws = workspace_store_->GetWorkspace(workspace_id);
  if (!ws.has_value()) {
    return;
  }

  auto dialog = std::make_unique<DeleteWorkspaceDialog>(ws->name);
  DeleteWorkspaceDialog* dialog_ptr = dialog.get();

  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  dialog_ptr->SetAcceptCallback(base::BindOnce(
      [](PureCloakSidePanelView* view, const std::string& ws_id) {
        view->workspace_store_->DeleteWorkspace(ws_id);
      },
      this, workspace_id));

  views::Widget* dialog_widget = views::Widget::CreateWindowWithParent(
      std::move(dialog), widget->GetNativeWindow());
  dialog_widget->Show();
}

BEGIN_METADATA(PureCloakSidePanelView)
END_METADATA

}  // namespace purecloak
