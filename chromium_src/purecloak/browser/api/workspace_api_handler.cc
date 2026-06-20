// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/api/workspace_api_handler.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "purecloak/browser/api/cdp_proxy_handler.h"
#include "purecloak/browser/profiles/workspace.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

namespace {

// Parse path segments from a request path.
// e.g. "/api/workspaces/abc/launch" → {"api", "workspaces", "abc", "launch"}
std::vector<std::string> ParsePath(const std::string& path) {
  std::string_view p = path;
  if (!p.empty() && p.front() == '/') {
    p.remove_prefix(1);
  }
  if (p.empty()) {
    return {};
  }
  return base::SplitString(p, "/", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

// Parse JSON body from a request. Returns empty dict on failure.
base::Value::Dict ParseJsonBody(const std::string& body) {
  if (body.empty()) {
    return base::Value::Dict();
  }
  auto parsed = base::JSONReader::Read(body);
  if (!parsed || !parsed->is_dict()) {
    return base::Value::Dict();
  }
  return std::move(parsed->GetDict());
}

}  // namespace

WorkspaceApiHandler::WorkspaceApiHandler(
    WorkspaceStore* store,
    RunningWorkspaceManager* manager)
    : workspace_store_(store),
      workspace_manager_(manager),
      cdp_proxy_(std::make_unique<CDPProxyHandler>(manager)) {
  DCHECK(workspace_store_);
  DCHECK(workspace_manager_);
}

WorkspaceApiHandler::~WorkspaceApiHandler() = default;

void WorkspaceApiHandler::OnConnect(int connection_id) {}

void WorkspaceApiHandler::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DispatchRequest(connection_id, info);
}

void WorkspaceApiHandler::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Route WebSocket upgrades to CDP proxy.
  auto segments = ParsePath(info.path);
  // /api/workspaces/{id}/cdp/...
  if (segments.size() >= 4 && segments[0] == "api" &&
      segments[1] == "workspaces") {
    // Reconstruct subpath after /cdp/
    // segments = [api, workspaces, {id}, cdp, ...]
    if (segments[3] == "cdp") {
      // Accept the upgrade so we can relay.
      if (server_) {
        server_->AcceptWebSocket(connection_id, info);
      }
      std::string subpath;
      for (size_t i = 4; i < segments.size(); ++i) {
        subpath += "/" + segments[i];
      }
      cdp_proxy_->HandleWebSocketUpgrade(connection_id, segments[2],
                                          subpath, info, this);
      return;
    }
  }

  SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
            "WebSocket endpoint not found");
}

void WorkspaceApiHandler::OnWebSocketMessage(int connection_id,
                                              std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdp_proxy_->OnWebSocketMessage(connection_id, std::move(data));
}

void WorkspaceApiHandler::OnClose(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdp_proxy_->OnClientDisconnect(connection_id);
}

// --- Route Dispatch ---

void WorkspaceApiHandler::DispatchRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  auto segments = ParsePath(info.path);

  if (segments.empty()) {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "No route matches");
    return;
  }

  // All API routes start with /api
  if (segments[0] != "api") {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "Unknown API endpoint");
    return;
  }

  // GET /api/status
  if (segments.size() == 2 && segments[1] == "status" &&
      info.method == "GET") {
    HandleSystemStatus(connection_id);
    return;
  }

  // /api/workspaces/...
  if (segments.size() >= 2 && segments[1] == "workspaces") {
    if (segments.size() == 2 && info.method == "GET") {
      HandleGetAllWorkspaces(connection_id);
      return;
    }
    if (segments.size() == 2 && info.method == "POST") {
      auto body = ParseJsonBody(info.data);
      HandleCreateWorkspace(connection_id, std::move(body));
      return;
    }
    if (segments.size() >= 3) {
      const std::string& ws_id = segments[2];
      if (segments.size() == 3 && info.method == "GET") {
        HandleGetWorkspace(connection_id, ws_id);
        return;
      }
      if (segments.size() == 3 && info.method == "PUT") {
        auto body = ParseJsonBody(info.data);
        HandleUpdateWorkspace(connection_id, ws_id, std::move(body));
        return;
      }
      if (segments.size() == 3 && info.method == "DELETE") {
        HandleDeleteWorkspace(connection_id, ws_id);
        return;
      }
      if (segments.size() == 4 && segments[3] == "launch" &&
          info.method == "POST") {
        HandleLaunchWorkspace(connection_id, ws_id);
        return;
      }
      if (segments.size() == 4 && segments[3] == "stop" &&
          info.method == "POST") {
        HandleStopWorkspace(connection_id, ws_id);
        return;
      }
      if (segments.size() == 4 && segments[3] == "status" &&
          info.method == "GET") {
        HandleGetWorkspaceStatus(connection_id, ws_id);
        return;
      }
      // CDP proxy HTTP endpoints (e.g. /json/version, /json/list)
      if (segments.size() >= 4 && segments[3] == "cdp") {
        std::string subpath;
        for (size_t i = 4; i < segments.size(); ++i) {
          subpath += "/" + segments[i];
        }
        HandleCDPRequest(connection_id, ws_id, subpath, info);
        return;
      }
    }
  }

  SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
            "No route matches " + info.path);
}

// --- Response Helpers ---

void WorkspaceApiHandler::SendJson(int connection_id,
                                    net::HttpStatusCode status,
                                    const base::Value& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!server_) {
    LOG(ERROR) << "SendJson: server_ is null";
    return;
  }
  std::string json;
  base::JSONWriter::Write(response, &json);
  server_->SendResponse(connection_id, status, json,
                         "application/json; charset=UTF-8");
}

void WorkspaceApiHandler::SendError(int connection_id,
                                     net::HttpStatusCode status,
                                     const std::string& code,
                                     const std::string& message) {
  base::Value::Dict error_detail;
  error_detail.Set("code", code);
  error_detail.Set("message", message);

  base::Value::Dict error;
  error.Set("success", false);
  error.Set("error", std::move(error_detail));

  SendJson(connection_id, status, base::Value(std::move(error)));
}

void WorkspaceApiHandler::SendSuccess(int connection_id,
                                       base::Value data,
                                       net::HttpStatusCode status) {
  base::Value::Dict response;
  response.Set("success", true);
  if (data.is_dict()) {
    // Merge the data dict into the response
    response.Set("data", std::move(data.GetDict()));
  } else {
    response.Set("data", std::move(data));
  }
  SendJson(connection_id, status, base::Value(std::move(response)));
}

// --- Cross-thread response helpers ---

void WorkspaceApiHandler::RespondWithWorkspaceList(
    int connection_id,
    std::vector<base::Value::Dict> workspaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::List list;
  for (auto& ws : workspaces) {
    list.Append(std::move(ws));
  }
  base::Value::Dict result;
  result.Set("success", true);
  result.Set("data", std::move(list));
  SendJson(connection_id, net::HTTP_OK, base::Value(std::move(result)));
}

void WorkspaceApiHandler::RespondWithWorkspace(
    int connection_id,
    std::optional<base::Value::Dict> ws) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ws.has_value()) {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "Workspace not found");
    return;
  }
  base::Value::Dict result;
  result.Set("success", true);
  result.Set("data", std::move(*ws));
  SendJson(connection_id, net::HTTP_OK, base::Value(std::move(result)));
}

void WorkspaceApiHandler::RespondWithStatus(int connection_id,
                                             bool success,
                                             base::Value::Dict data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data.Set("success", success);
  SendJson(connection_id, success ? net::HTTP_OK : net::HTTP_BAD_REQUEST,
           base::Value(std::move(data)));
}

// --- REST Handlers ---

void WorkspaceApiHandler::HandleGetAllWorkspaces(int connection_id) {
  // Capture the API thread task runner for posting back.
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto workspaces = store->GetAllWorkspaces();
            std::vector<base::Value::Dict> dicts;
            for (const auto& ws : workspaces) {
              dicts.push_back(ws.ToDict());
            }
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspaceList,
                               weak_this, connection_id, std::move(dicts)));
          },
          base::Unretained(workspace_store_), connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleCreateWorkspace(
    int connection_id,
    base::Value::Dict body) {
  // Validate required fields
  std::string* name = body.FindString("name");
  if (!name || name->empty()) {
    SendError(connection_id, net::HTTP_BAD_REQUEST, "INVALID_REQUEST",
              "Missing required field: name");
    return;
  }

  std::string type_str = body.FindString("type") ? *body.FindString("type")
                                                  : "normal";
  Workspace::Type type = Workspace::StringToType(type_str);

  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, std::string name, Workspace::Type type,
             int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto ws = Workspace::CreateBasic(name, type);

            // Apply optional overrides from body
            // (other fields can be applied via subsequent PUT)

            ws = store->CreateWorkspace(std::move(ws));

            auto ws_dict = ws.ToDict();
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspace,
                               weak_this, connection_id, std::move(ws_dict)));
          },
          base::Unretained(workspace_store_), *name, type, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleGetWorkspace(int connection_id,
                                              const std::string& ws_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, std::string ws_id, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto ws = store->GetWorkspace(ws_id);
            std::optional<base::Value::Dict> result;
            if (ws.has_value()) {
              result = ws->ToDict();
            }
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspace,
                               weak_this, connection_id, std::move(result)));
          },
          base::Unretained(workspace_store_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleUpdateWorkspace(
    int connection_id,
    const std::string& ws_id,
    base::Value::Dict body) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, std::string ws_id,
             base::Value::Dict body, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            // Get existing workspace
            auto existing = store->GetWorkspace(ws_id);
            if (!existing.has_value()) {
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspace,
                                 weak_this, connection_id, std::nullopt));
              return;
            }

            Workspace ws = std::move(*existing);

            // Apply updates from body
            if (std::string* v = body.FindString("name")) {
              ws.name = *v;
            }

            // Apply full-field update
            bool success = store->UpdateWorkspace(ws);
            std::optional<base::Value::Dict> result;
            if (success) {
              result = ws.ToDict();
            }
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspace,
                               weak_this, connection_id, std::move(result)));
          },
          base::Unretained(workspace_store_), ws_id, std::move(body),
          connection_id, std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleDeleteWorkspace(
    int connection_id,
    const std::string& ws_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, std::string ws_id, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            bool success = store->DeleteWorkspace(ws_id);
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithStatus,
                               weak_this, connection_id, success,
                               base::Value::Dict()));
          },
          base::Unretained(workspace_store_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleLaunchWorkspace(
    int connection_id,
    const std::string& ws_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, WorkspaceStore* store,
             std::string ws_id, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto ws = store->GetWorkspace(ws_id);
            if (!ws.has_value()) {
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(&WorkspaceApiHandler::RespondWithStatus,
                                 weak_this, connection_id, false,
                                 base::Value::Dict()));
              return;
            }

            manager->Start(
                *ws,
                base::BindOnce(
                    [](int connection_id,
                       scoped_refptr<base::SequencedTaskRunner> api_runner,
                       base::WeakPtr<WorkspaceApiHandler> weak_this,
                       bool success, const base::DictValue& result) {
                      api_runner->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &WorkspaceApiHandler::RespondWithStatus,
                              weak_this, connection_id, success,
                              result.Clone().TakeDict()));
                    },
                    connection_id, api_runner, weak_this));
          },
          base::Unretained(workspace_manager_),
          base::Unretained(workspace_store_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleStopWorkspace(
    int connection_id,
    const std::string& ws_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, std::string ws_id,
             int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            manager->Stop(ws_id);
            base::Value::Dict result;
            result.Set("workspace_id", ws_id);
            result.Set("status", "stopped");
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithStatus,
                               weak_this, connection_id, true,
                               std::move(result)));
          },
          base::Unretained(workspace_manager_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleGetWorkspaceStatus(
    int connection_id,
    const std::string& ws_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, std::string ws_id,
             int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto status = manager->GetStatusDict(ws_id);
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithStatus,
                               weak_this, connection_id, true,
                               std::move(status.TakeDict())));
              },
          base::Unretained(workspace_manager_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleSystemStatus(int connection_id) {
  base::Value::Dict result;
  result.Set("status", "running");
  result.Set("version", "1.0.0");

  // Get running workspace count from UI thread
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            size_t count = manager->RunningCount();
            size_t total = manager->GetAll().size();

            base::Value::Dict data;
            data.Set("status", "running");
            data.Set("version", "1.0.0");
            data.Set("running_workspaces", static_cast<int>(count));
            data.Set("total_workspaces", static_cast<int>(total));

            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&WorkspaceApiHandler::RespondWithStatus,
                               weak_this, connection_id, true,
                               std::move(data)));
          },
          base::Unretained(workspace_manager_), connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleCDPRequest(
    int connection_id,
    const std::string& ws_id,
    const std::string& subpath,
    const net::HttpServerRequestInfo& info) {
  // For HTTP CDP endpoints like /json/version and /json/list,
  // we forward the request to the workspace's CDP port via the proxy.
  // The CDP proxy can handle both HTTP and WS.
  if (!cdp_proxy_) {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "CDP proxy not available");
    return;
  }

  // For now, CDP HTTP endpoints require the workspace to be running.
  // We look up the CDP port from the workspace manager on the UI thread.
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, std::string ws_id,
             std::string subpath, int connection_id,
             net::HttpServerRequestInfo info,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto* running = manager->Get(ws_id);
            if (!running || running->status != RunningWorkspace::Status::kRunning) {
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](int connection_id,
                         base::WeakPtr<WorkspaceApiHandler> weak_this) {
                        if (weak_this) {
                          weak_this->SendError(
                              connection_id, net::HTTP_BAD_REQUEST,
                              "NOT_RUNNING",
                              "Workspace is not running");
                        }
                      },
                      connection_id, weak_this));
              return;
            }

            // Build target URL from the workspace CDP port.
            std::string target_url =
                "http://127.0.0.1:" + base::NumberToString(running->cdp_port) +
                subpath;

            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](int connection_id, std::string target_url,
                       base::WeakPtr<WorkspaceApiHandler> weak_this) {
                      if (weak_this) {
                        // Stub: For now return the CDP URL so the client
                        // can connect directly.
                        base::Value::Dict result;
                        result.Set("webSocketDebuggerUrl",
                                    target_url);
                        weak_this->RespondWithStatus(
                            connection_id, true, std::move(result));
                      }
                    },
                    connection_id, target_url, weak_this));
          },
          base::Unretained(workspace_manager_), ws_id, subpath, connection_id,
          info, std::move(api_runner), std::move(weak_this)));
}

}  // namespace purecloak
