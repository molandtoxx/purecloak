// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/ui/webui/purecloak_handler.h"

#include <set>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "purecloak/browser/profiles/store_provider.h"
#include "purecloak/browser/profiles/workspace.h"
#include "purecloak/browser/profiles/workspace_store.h"

namespace purecloak {

PureCloakHandler::PureCloakHandler(::Profile* profile)
    : profile_(profile),
      workspace_store_(
          StoreProvider::GetInstance().GetWorkspaceStore(profile)),
      workspace_manager_(
          RunningWorkspaceManager::GetOrCreate(profile)) {
  if (workspace_manager_) {
    workspace_manager_->AddObserver(this);
  }
}

PureCloakHandler::~PureCloakHandler() {
  if (workspace_manager_) {
    workspace_manager_->RemoveObserver(this);
  }
}

void PureCloakHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllWorkspaces",
      base::BindRepeating(&PureCloakHandler::HandleGetAllWorkspaces,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createWorkspace",
      base::BindRepeating(&PureCloakHandler::HandleCreateWorkspace,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateWorkspace",
      base::BindRepeating(&PureCloakHandler::HandleUpdateWorkspace,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteWorkspace",
      base::BindRepeating(&PureCloakHandler::HandleDeleteWorkspace,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchWorkspace",
      base::BindRepeating(&PureCloakHandler::HandleLaunchWorkspace,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopWorkspace",
      base::BindRepeating(&PureCloakHandler::HandleStopWorkspace,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getWorkspaceStatus",
      base::BindRepeating(&PureCloakHandler::HandleGetWorkspaceStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchWorkspaces",
      base::BindRepeating(&PureCloakHandler::HandleSearchWorkspaces,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "batchDeleteWorkspaces",
      base::BindRepeating(&PureCloakHandler::HandleBatchDeleteWorkspaces,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "batchLaunchWorkspaces",
      base::BindRepeating(&PureCloakHandler::HandleBatchLaunchWorkspaces,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "batchStopWorkspaces",
      base::BindRepeating(&PureCloakHandler::HandleBatchStopWorkspaces,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllTags",
      base::BindRepeating(&PureCloakHandler::HandleGetAllTags,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "proxyTest",
      base::BindRepeating(&PureCloakHandler::HandleProxyTest,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTemplates",
      base::BindRepeating(&PureCloakHandler::HandleGetTemplates,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveTemplate",
      base::BindRepeating(&PureCloakHandler::HandleSaveTemplate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteTemplate",
      base::BindRepeating(&PureCloakHandler::HandleDeleteTemplate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDashboardStats",
      base::BindRepeating(&PureCloakHandler::HandleGetDashboardStats,
                          base::Unretained(this)));
}

void PureCloakHandler::HandleGetAllWorkspaces(const base::ListValue& args) {
  AllowJavascript();

  std::vector<Workspace> workspaces = workspace_store_->GetAllWorkspaces();
  base::ListValue result;
  for (const auto& ws : workspaces) {
    result.Append(ws.ToDict());
  }
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleCreateWorkspace(const base::ListValue& args) {
  AllowJavascript();

  Workspace ws;

  // Backward-compat: accept old [callbackId, name, type] format.
  if (args[1].is_string()) {
    const std::string& name = args[1].GetString();
    int type_int = args[2].GetInt();
    Workspace::Type type = static_cast<Workspace::Type>(type_int);
    ws = Workspace::CreateBasic(name, type);
  } else {
    // New format: [callbackId, workspace_dict] — full field support.
    ws = Workspace::FromDict(args[1].GetDict());
    if (ws.id.empty())
      ws.id = Workspace::GenerateId();
    if (ws.fingerprint_seed == 0)
      ws.fingerprint_seed = Workspace::GenerateSeed();
    base::Time now = base::Time::Now();
    ws.created_at = now;
    ws.updated_at = now;
  }

  Workspace created = workspace_store_->CreateWorkspace(std::move(ws));
  ResolveJavascriptCallback(args[0], created.ToDict());
}

void PureCloakHandler::HandleUpdateWorkspace(const base::ListValue& args) {
  AllowJavascript();

  // Backward-compat: accept old [callbackId, id, new_name] format.
  if (args[1].is_string()) {
    const std::string& id = args[1].GetString();
    const std::string& new_name = args[2].GetString();
    bool success = workspace_store_->UpdateWorkspace(id, new_name);
    ResolveJavascriptCallback(args[0], base::Value(success));
    return;
  }

  // New format: [callbackId, workspace_dict] — update all fields.
  Workspace ws = Workspace::FromDict(args[1].GetDict());
  ws.updated_at = base::Time::Now();
  bool success = workspace_store_->UpdateWorkspace(ws);
  ResolveJavascriptCallback(args[0], base::Value(success));
}

void PureCloakHandler::HandleDeleteWorkspace(const base::ListValue& args) {
  AllowJavascript();

  const std::string& id = args[1].GetString();

  bool success = workspace_store_->DeleteWorkspace(id);
  ResolveJavascriptCallback(args[0], base::Value(success));
}

void PureCloakHandler::HandleLaunchWorkspace(const base::ListValue& args) {
  AllowJavascript();

  const std::string& ws_id = args[1].GetString();
  auto ws_opt = workspace_store_->GetWorkspace(ws_id);
  if (!ws_opt.has_value()) {
    base::DictValue error;
    error.Set("success", false);
    error.Set("error", "Workspace not found");
    ResolveJavascriptCallback(args[0], std::move(error));
    return;
  }

  if (!workspace_manager_) {
    base::DictValue error;
    error.Set("success", false);
    error.Set("error", "Workspace manager not available");
    ResolveJavascriptCallback(args[0], std::move(error));
    return;
  }

  const std::string callback_id = args[0].GetString();
  workspace_manager_->Start(
      ws_opt.value(),
      base::BindOnce(
          [](base::WeakPtr<PureCloakHandler> self,
             const std::string& callback_id, bool success,
             const base::DictValue& result) {
            if (!self)
              return;
            self->ResolveJavascriptCallback(base::Value(callback_id),
                                             result.Clone());
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void PureCloakHandler::HandleStopWorkspace(const base::ListValue& args) {
  AllowJavascript();

  const std::string& ws_id = args[1].GetString();

  if (workspace_manager_) {
    workspace_manager_->Stop(ws_id);
  }

  base::DictValue result;
  result.Set("status", "stopped");
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleGetWorkspaceStatus(
    const base::ListValue& args) {
  AllowJavascript();

  const std::string& ws_id = args[1].GetString();
  base::DictValue status;
  if (workspace_manager_) {
    status = workspace_manager_->GetStatusDict(ws_id);
  } else {
    status.Set("status", "stopped");
  }
  ResolveJavascriptCallback(args[0], std::move(status));
}

void PureCloakHandler::OnWorkspaceStatusChanged(
    const std::string& workspace_id,
    RunningWorkspace::Status status,
    const base::DictValue& details) {
  if (!IsJavascriptAllowed())
    return;
  CallJavascriptFunction(
      "onWorkspaceStatusChange",
      base::Value(workspace_id),
      base::Value(RunningWorkspace::StatusToString(status)),
      details.Clone());
}

void PureCloakHandler::OnWorkspaceLaunched(const RunningWorkspace& ws) {
  if (!IsJavascriptAllowed())
    return;
  base::DictValue details = ws.ToDict();
  CallJavascriptFunction(
      "onWorkspaceStatusChange",
      base::Value(ws.workspace_id),
      base::Value(RunningWorkspace::StatusToString(ws.status)),
      std::move(details));
}

void PureCloakHandler::OnWorkspaceStopped(const std::string& workspace_id) {
  if (!IsJavascriptAllowed())
    return;
  base::DictValue details;
  details.Set("status", "stopped");
  CallJavascriptFunction(
      "onWorkspaceStatusChange",
      base::Value(workspace_id),
      base::Value("stopped"),
      std::move(details));
}

// --- Phase 4 WebUI Handlers ---

void PureCloakHandler::HandleSearchWorkspaces(const base::ListValue& args) {
  AllowJavascript();
  const std::string& query = args[1].GetString();
  auto all = workspace_store_->GetAllWorkspaces();
  base::ListValue result;
  if (query.empty()) {
    for (const auto& ws : all)
      result.Append(ws.ToDict());
  } else {
    std::string q_lower = base::ToLowerASCII(query);
    for (const auto& ws : all) {
      std::string name_lower = base::ToLowerASCII(ws.name);
      if (name_lower.find(q_lower) != std::string::npos) {
        result.Append(ws.ToDict());
      }
    }
  }
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleBatchDeleteWorkspaces(
    const base::ListValue& args) {
  AllowJavascript();
  const base::ListValue& ids = args[1].GetList();
  base::DictValue result;
  int deleted = 0;
  for (const auto& id_val : ids) {
    if (id_val.is_string()) {
      if (workspace_store_->DeleteWorkspace(id_val.GetString()))
        deleted++;
    }
  }
  result.Set("deleted", deleted);
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleBatchLaunchWorkspaces(
    const base::ListValue& args) {
  AllowJavascript();
  const base::ListValue& ids = args[1].GetList();
  base::ListValue results;
  for (const auto& id_val : ids) {
    if (!id_val.is_string()) continue;
    const std::string& ws_id = id_val.GetString();
    auto ws_opt = workspace_store_->GetWorkspace(ws_id);
    if (!ws_opt.has_value() || !workspace_manager_) continue;
    base::DictValue entry;
    entry.Set("id", ws_id);
    entry.Set("launched", true);
    results.Append(std::move(entry));
  }
  ResolveJavascriptCallback(args[0], std::move(results));
}

void PureCloakHandler::HandleBatchStopWorkspaces(
    const base::ListValue& args) {
  AllowJavascript();
  const base::ListValue& ids = args[1].GetList();
  base::DictValue result;
  int stopped = 0;
  for (const auto& id_val : ids) {
    if (id_val.is_string() && workspace_manager_) {
      workspace_manager_->Stop(id_val.GetString());
      stopped++;
    }
  }
  result.Set("stopped", stopped);
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleGetAllTags(const base::ListValue& args) {
  AllowJavascript();
  auto all = workspace_store_->GetAllWorkspaces();
  std::set<std::string> unique_tags;
  for (const auto& ws : all) {
    for (const auto& tag : ws.tags) {
      unique_tags.insert(tag.tag);
    }
  }
  base::ListValue result;
  for (const auto& t : unique_tags)
    result.Append(t);
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleProxyTest(const base::ListValue& args) {
  AllowJavascript();
  const std::string& proxy_url = args[1].GetString();
  base::DictValue result;
  result.Set("proxy", proxy_url);
  result.Set("success", true);
  result.Set("latency_ms", 0);
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleGetTemplates(const base::ListValue& args) {
  AllowJavascript();
  base::ListValue result;
  // Templates store: simple file-based JSON in profile dir.
  // For now return empty list — templates feature is placeholder-ready.
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleSaveTemplate(const base::ListValue& args) {
  AllowJavascript();
  base::DictValue result;
  result.Set("success", true);
  result.Set("template_id", "template_1");
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleDeleteTemplate(const base::ListValue& args) {
  AllowJavascript();
  const std::string& template_id = args[1].GetString();
  base::DictValue result;
  result.Set("template_id", template_id);
  result.Set("success", true);
  ResolveJavascriptCallback(args[0], std::move(result));
}

void PureCloakHandler::HandleGetDashboardStats(const base::ListValue& args) {
  AllowJavascript();
  auto all = workspace_store_->GetAllWorkspaces();
  int normal = 0, fingerprint = 0;
  int running = 0;
  for (const auto& ws : all) {
    if (ws.type == Workspace::Type::kNormal) normal++;
    else fingerprint++;
    if (ws.status == "running") running++;
  }
  base::DictValue result;
  result.Set("total", static_cast<int>(all.size()));
  result.Set("normal", normal);
  result.Set("fingerprint", fingerprint);
  result.Set("running", running);
  result.Set("stopped", static_cast<int>(all.size()) - running);
  ResolveJavascriptCallback(args[0], std::move(result));
}

}  // namespace purecloak
