// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_VIEW_H_
#define PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
class LabelButton;
}  // namespace views

namespace purecloak {

class PureCloakSidePanelView : public views::View,
                               public WorkspaceStoreObserver {
  METADATA_HEADER(PureCloakSidePanelView, views::View)

 public:
  explicit PureCloakSidePanelView(WorkspaceStore* workspace_store);
  ~PureCloakSidePanelView() override;

  // Prevent copy/move.
  PureCloakSidePanelView(const PureCloakSidePanelView&) = delete;
  PureCloakSidePanelView& operator=(const PureCloakSidePanelView&) = delete;

 private:
  // Observer callbacks.
  void OnWorkspacesChanged() override;

  // Build the workspace list UI from store data.
  void RebuildWorkspaceList();

  // Handle workspace creation.
  void OnCreateWorkspaceClicked();

  // Handle workspace actions (rename, delete).
  void OnRenameWorkspace(const std::string& workspace_id);
  void OnDeleteWorkspace(const std::string& workspace_id);

  raw_ptr<WorkspaceStore> workspace_store_;

  raw_ptr<views::LabelButton> create_workspace_button_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::View> container_ = nullptr;

  base::ScopedObservation<WorkspaceStore, WorkspaceStoreObserver>
      workspace_observation_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_VIEWS_SIDE_PANEL_PURECLOAK_SIDE_PANEL_VIEW_H_
