// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_HANDLER_H_
#define PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "purecloak/browser/workspace_launcher.h"

class Profile;

namespace purecloak {

class WorkspaceStore;
struct Workspace;

// WebUIMessageHandler for //purecloak/.
// Bridges JavaScript frontend with WorkspaceStore.
class PureCloakHandler : public content::WebUIMessageHandler,
                         public RunningWorkspaceObserver {
 public:
  explicit PureCloakHandler(::Profile* profile);
  PureCloakHandler(const PureCloakHandler&) = delete;
  PureCloakHandler& operator=(const PureCloakHandler&) = delete;
  ~PureCloakHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // RunningWorkspaceObserver:
  void OnWorkspaceStatusChanged(const std::string& workspace_id,
                                RunningWorkspace::Status status,
                                const base::DictValue& details) override;
  void OnWorkspaceLaunched(const RunningWorkspace& ws) override;
  void OnWorkspaceStopped(const std::string& workspace_id) override;

  // JS->C++ message handlers.
  void HandleGetAllWorkspaces(const base::ListValue& args);
  void HandleCreateWorkspace(const base::ListValue& args);
  void HandleUpdateWorkspace(const base::ListValue& args);
  void HandleDeleteWorkspace(const base::ListValue& args);
  void HandleLaunchWorkspace(const base::ListValue& args);
  void HandleStopWorkspace(const base::ListValue& args);
  void HandleGetWorkspaceStatus(const base::ListValue& args);

  // Phase 4 WebUI handlers.
  void HandleSearchWorkspaces(const base::ListValue& args);
  void HandleBatchDeleteWorkspaces(const base::ListValue& args);
  void HandleBatchLaunchWorkspaces(const base::ListValue& args);
  void HandleBatchStopWorkspaces(const base::ListValue& args);
  void HandleGetAllTags(const base::ListValue& args);
  void HandleProxyTest(const base::ListValue& args);
  void HandleGetTemplates(const base::ListValue& args);
  void HandleSaveTemplate(const base::ListValue& args);
  void HandleDeleteTemplate(const base::ListValue& args);
  void HandleGetDashboardStats(const base::ListValue& args);

 private:
  raw_ptr<::Profile> profile_;
  raw_ptr<WorkspaceStore> workspace_store_;
  raw_ptr<RunningWorkspaceManager> workspace_manager_;

  base::WeakPtrFactory<PureCloakHandler> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_UI_WEBUI_PURECLOAK_HANDLER_H_
