// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/api/workspace_api_handler.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "purecloak/browser/api/cdp_proxy_handler.h"
#include "purecloak/browser/profiles/workspace.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

namespace {

constexpr net::NetworkTrafficAnnotationTag kApiTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("purecloak_api_handler", R"(
      semantics {
        sender: "PureCloak API Handler"
        description: "HTTP responses for PureCloak workspace management API"
        trigger: "Request to PureCloak API server"
        data: "Workspace configuration and status data"
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
      })");

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
base::DictValue ParseJsonBody(const std::string& body) {
  if (body.empty()) {
    return base::DictValue();
  }
  auto parsed = base::JSONReader::ReadDict(body, 0);
  if (!parsed) {
    return base::DictValue();
  }
  return std::move(*parsed);
}

// Apply optional workspace fields from a JSON body dict to a Workspace.
// Uses the same field-name constants as Workspace::FromDict.
void ApplyBodyToWorkspace(Workspace& ws, const base::DictValue& body) {
  if (const std::string* v = body.FindString("name"))
    ws.name = *v;
  if (const std::string* v = body.FindString("user_agent"))
    ws.user_agent = *v;
  if (std::optional<int> v = body.FindInt("screen_width"))
    ws.screen_width = *v;
  if (std::optional<int> v = body.FindInt("screen_height"))
    ws.screen_height = *v;
  if (const std::string* v = body.FindString("gpu_vendor"))
    ws.gpu_vendor = *v;
  if (const std::string* v = body.FindString("gpu_renderer"))
    ws.gpu_renderer = *v;
  if (std::optional<int> v = body.FindInt("hardware_concurrency"))
    ws.hardware_concurrency = *v;
  if (const std::string* v = body.FindString("platform"))
    ws.platform = *v;
  if (const std::string* v = body.FindString("color_scheme"))
    ws.color_scheme = *v;
  if (const std::string* v = body.FindString("proxy"))
    ws.proxy = *v;
  if (const std::string* v = body.FindString("timezone"))
    ws.timezone = *v;
  if (const std::string* v = body.FindString("locale"))
    ws.locale = *v;
  if (std::optional<bool> v = body.FindBool("geoip"))
    ws.geoip = *v;
  if (const std::string* v = body.FindString("default_tab_title"))
    ws.default_tab_title = *v;
  if (const std::string* v = body.FindString("notes"))
    ws.notes = *v;
  if (std::optional<bool> v = body.FindBool("humanize"))
    ws.humanize = *v;
  if (const std::string* v = body.FindString("human_preset"))
    ws.human_preset = *v;
  if (std::optional<bool> v = body.FindBool("headless"))
    ws.headless = *v;
  if (std::optional<bool> v = body.FindBool("clipboard_sync"))
    ws.clipboard_sync = *v;
  if (std::optional<bool> v = body.FindBool("auto_launch"))
    ws.auto_launch = *v;
  if (std::optional<int> v = body.FindInt("color"))
    ws.color = static_cast<uint32_t>(*v);
}

}  // namespace

WorkspaceApiHandler::WorkspaceApiHandler(
    WorkspaceStore* store,
    RunningWorkspaceManager* manager,
    const std::string& api_token)
    : workspace_store_(store),
      workspace_manager_(manager),
      api_token_(api_token),
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
        server_->AcceptWebSocket(connection_id, info,
                                  kApiTrafficAnnotation);
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
  // Bearer token authentication check.
  if (!api_token_.empty()) {
    bool auth_ok = false;
    auto auth_it = info.headers.find("Authorization");
    if (auth_it != info.headers.end()) {
      std::string expected = "Bearer " + api_token_;
      if (auth_it->second == expected) {
        auth_ok = true;
      }
    }
    if (!auth_ok) {
      SendError(connection_id, net::HTTP_UNAUTHORIZED, "UNAUTHORIZED",
                "Missing or invalid Authorization: Bearer token");
      return;
    }
  }

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
      // Export all workspaces (GET /api/workspaces/export)
      if (segments.size() == 3 && segments[2] == "export" &&
          info.method == "GET") {
        HandleExportWorkspaces(connection_id);
        return;
      }
      // Import workspaces (POST /api/workspaces/import)
      if (segments.size() == 3 && segments[2] == "import" &&
          info.method == "POST") {
        HandleImportWorkspaces(connection_id, info.data);
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
  server_->Send(connection_id, status, json,
                "application/json; charset=UTF-8",
                kApiTrafficAnnotation);
}

void WorkspaceApiHandler::SendError(int connection_id,
                                     net::HttpStatusCode status,
                                     const std::string& code,
                                     const std::string& message) {
  base::DictValue error_detail;
  error_detail.Set("code", code);
  error_detail.Set("message", message);

  base::DictValue error;
  error.Set("success", false);
  error.Set("error", std::move(error_detail));

  SendJson(connection_id, status, base::Value(std::move(error)));
}

void WorkspaceApiHandler::SendSuccess(int connection_id,
                                       base::Value data,
                                       net::HttpStatusCode status) {
  base::DictValue response;
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
    std::vector<base::DictValue> workspaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ListValue list;
  for (auto& ws : workspaces) {
    list.Append(std::move(ws));
  }
  base::DictValue result;
  result.Set("success", true);
  result.Set("data", std::move(list));
  SendJson(connection_id, net::HTTP_OK, base::Value(std::move(result)));
}

void WorkspaceApiHandler::RespondWithWorkspace(
    int connection_id,
    std::optional<base::DictValue> ws) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ws.has_value()) {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "Workspace not found");
    return;
  }
  base::DictValue result;
  result.Set("success", true);
  result.Set("data", std::move(*ws));
  SendJson(connection_id, net::HTTP_OK, base::Value(std::move(result)));
}

void WorkspaceApiHandler::RespondWithStatus(int connection_id,
                                             bool success,
                                             base::DictValue data) {
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
            std::vector<base::DictValue> dicts;
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
    base::DictValue body) {
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
              base::DictValue body,
              int connection_id,
              scoped_refptr<base::SequencedTaskRunner> api_runner,
              base::WeakPtr<WorkspaceApiHandler> weak_this) {
             auto ws = Workspace::CreateBasic(name, type);

             // Apply optional overrides from body
             ApplyBodyToWorkspace(ws, body);

             ws = store->CreateWorkspace(std::move(ws));

             auto ws_dict = ws.ToDict();
             api_runner->PostTask(
                 FROM_HERE,
                 base::BindOnce(&WorkspaceApiHandler::RespondWithWorkspace,
                                weak_this, connection_id, std::move(ws_dict)));
           },
           base::Unretained(workspace_store_), *name, type, std::move(body),
           connection_id, std::move(api_runner), std::move(weak_this)));
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
            std::optional<base::DictValue> result;
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
    base::DictValue body) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, std::string ws_id,
             base::DictValue body, int connection_id,
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
            ApplyBodyToWorkspace(ws, body);

            // Apply full-field update
            bool success = store->UpdateWorkspace(ws);
            std::optional<base::DictValue> result;
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
                               base::DictValue()));
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
                                 base::DictValue()));
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
                               result.Clone()));
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
            base::DictValue result;
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
                                std::move(status)));
              },
          base::Unretained(workspace_manager_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleSystemStatus(int connection_id) {
  base::DictValue result;
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

            base::DictValue data;
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
  if (!cdp_proxy_) {
    SendError(connection_id, net::HTTP_NOT_FOUND, "NOT_FOUND",
              "CDP proxy not available");
    return;
  }

  // Look up the CDP port and forward the HTTP request to the child.
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
              // Retry once after a delay — the workspace may be restarting.
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(&WorkspaceApiHandler::RetryCDPRequest,
                                 weak_this, connection_id, ws_id, subpath,
                                 info));
              return;
            }

            int cdp_port = running->cdp_port;

            // Build a minimal HTTP request to forward to the child CDP.
            std::string forward_request =
                info.method + " " + subpath + " HTTP/1.1\r\n" +
                "Host: 127.0.0.1:" + base::NumberToString(cdp_port) + "\r\n" +
                "Connection: close\r\n" +
                "\r\n";

            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](int connection_id, int cdp_port,
                       std::string forward_request,
                       base::WeakPtr<WorkspaceApiHandler> weak_this) {
                      if (!weak_this)
                        return;

                      // Connect to the child CDP port and send the request.
                      int sock = socket(AF_INET, SOCK_STREAM, 0);
                      if (sock < 0) {
                        weak_this->SendError(
                            connection_id, net::HTTP_INTERNAL_SERVER_ERROR,
                            "PROXY_ERROR", "Failed to create socket");
                        return;
                      }

                      struct sockaddr_in addr = {};
                      addr.sin_family = AF_INET;
                      addr.sin_port = htons(cdp_port);
                      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

                      struct timeval tv = {2, 0};  // 2s timeout.
                      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                      setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

                      if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                        close(sock);
                        weak_this->SendError(
                            connection_id, net::HTTP_BAD_GATEWAY,
                            "PROXY_ERROR", "Failed to connect to child CDP");
                        return;
                      }

                      // Send the forwarded request.
                      send(sock, forward_request.data(),
                           forward_request.size(), 0);

                      // Read response (up to 64KB).
                      std::string response;
                      char buf[4096];
                      int n;
                      while ((n = read(sock, buf, sizeof(buf))) > 0) {
                        response.append(buf, n);
                      }
                      close(sock);

                      if (response.empty()) {
                        weak_this->SendError(
                            connection_id, net::HTTP_BAD_GATEWAY,
                            "PROXY_ERROR", "Empty response from child CDP");
                        return;
                      }

                      // Parse HTTP status from the first line.
                      int http_status = net::HTTP_OK;
                      auto space1 = response.find(' ');
                      if (space1 != std::string::npos) {
                        auto space2 = response.find(' ', space1 + 1);
                        if (space2 != std::string::npos) {
                          int code = 0;
                          if (base::StringToInt(
                                  response.substr(space1 + 1,
                                                   space2 - space1 - 1),
                                  &code)) {
                            http_status = static_cast<net::HttpStatusCode>(code);
                          }
                        }
                      }

                      // Strip HTTP headers and send body as JSON.
                      auto header_end = response.find("\r\n\r\n");
                      if (header_end != std::string::npos) {
                        response = response.substr(header_end + 4);
                      }

                      // Try to parse as JSON for a clean response.
                      auto parsed = base::JSONReader::Read(response, base::JSON_PARSE_RFC);
                      if (parsed && parsed->is_dict()) {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(*parsed));
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      } else if (parsed && parsed->is_list()) {
                        // /json/list returns an array.
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(*parsed));
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      } else {
                        // Non-JSON response: wrap as text.
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("raw", response);
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      }
                    },
                    connection_id, cdp_port, std::move(forward_request),
                    weak_this));
          },
          base::Unretained(workspace_manager_), ws_id, subpath, connection_id,
          info, std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::RetryCDPRequest(
    int connection_id,
    const std::string& ws_id,
    const std::string& subpath,
    net::HttpServerRequestInfo info) {
  // Re-check workspace status with a fresh trip to the UI thread.
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, std::string ws_id,
             std::string subpath, int connection_id,
             net::HttpServerRequestInfo info,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto* running = manager->Get(ws_id);
            if (!running || running->status !=
                                RunningWorkspace::Status::kRunning) {
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](int connection_id,
                         base::WeakPtr<WorkspaceApiHandler> weak_this) {
                        if (weak_this) {
                          weak_this->SendError(
                              connection_id, net::HTTP_BAD_REQUEST,
                              "NOT_RUNNING",
                              "Workspace is not running after retry");
                        }
                      },
                      connection_id, weak_this));
              return;
            }

            int cdp_port = running->cdp_port;
            std::string forward_request =
                info.method + " " + subpath + " HTTP/1.1\r\n"
                "Host: 127.0.0.1:" + base::NumberToString(cdp_port) + "\r\n"
                "Connection: close\r\n"
                "\r\n";

            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](int connection_id, int cdp_port,
                       std::string forward_request,
                       base::WeakPtr<WorkspaceApiHandler> weak_this) {
                      if (!weak_this)
                        return;
                      int sock = socket(AF_INET, SOCK_STREAM, 0);
                      if (sock < 0) {
                        weak_this->SendError(
                            connection_id, net::HTTP_INTERNAL_SERVER_ERROR,
                            "PROXY_ERROR", "Failed to create socket");
                        return;
                      }
                      struct sockaddr_in addr = {};
                      addr.sin_family = AF_INET;
                      addr.sin_port = htons(cdp_port);
                      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                      struct timeval tv = {2, 0};
                      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv,
                                 sizeof(tv));
                      setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv,
                                 sizeof(tv));
                      if (connect(sock, (struct sockaddr*)&addr,
                                  sizeof(addr)) < 0) {
                        close(sock);
                        weak_this->SendError(
                            connection_id, net::HTTP_BAD_GATEWAY,
                            "PROXY_ERROR",
                            "Failed to connect to child CDP (retry)");
                        return;
                      }
                      send(sock, forward_request.data(),
                           forward_request.size(), 0);
                      std::string response;
                      char buf[4096];
                      int n;
                      while ((n = read(sock, buf, sizeof(buf))) > 0) {
                        response.append(buf, n);
                      }
                      close(sock);
                      if (response.empty()) {
                        weak_this->SendError(
                            connection_id, net::HTTP_BAD_GATEWAY,
                            "PROXY_ERROR",
                            "Empty response from child CDP (retry)");
                        return;
                      }
                      int http_status = net::HTTP_OK;
                      auto space1 = response.find(' ');
                      if (space1 != std::string::npos) {
                        auto space2 = response.find(' ', space1 + 1);
                        if (space2 != std::string::npos) {
                          int code = 0;
                          if (base::StringToInt(
                                  response.substr(space1 + 1,
                                                   space2 - space1 - 1),
                                  &code)) {
                            http_status =
                                static_cast<net::HttpStatusCode>(code);
                          }
                        }
                      }
                      auto header_end = response.find("\r\n\r\n");
                      if (header_end != std::string::npos) {
                        response = response.substr(header_end + 4);
                      }
                      auto parsed = base::JSONReader::Read(response, base::JSON_PARSE_RFC);
                      if (parsed && parsed->is_dict()) {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(*parsed));
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      } else if (parsed && parsed->is_list()) {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(*parsed));
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      } else {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("raw", response);
                        weak_this->SendJson(
                            connection_id,
                            static_cast<net::HttpStatusCode>(http_status),
                            base::Value(std::move(result)));
                      }
                    },
                    connection_id, cdp_port, std::move(forward_request),
                    weak_this));
          },
          base::Unretained(workspace_manager_), ws_id, subpath,
          connection_id, info, std::move(api_runner), std::move(weak_this)),
      base::Milliseconds(1000));
}

void WorkspaceApiHandler::HandleExportWorkspaces(int connection_id) {
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            auto workspaces = store->GetAllWorkspaces();
            base::ListValue list;
            for (const auto& ws : workspaces) {
              list.Append(ws.ToDict());
            }
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](int connection_id, base::ListValue data,
                       base::WeakPtr<WorkspaceApiHandler> weak_this) {
                      if (weak_this) {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(data));
                        weak_this->SendJson(
                            connection_id, net::HTTP_OK,
                            base::Value(std::move(result)));
                      }
                    },
                    connection_id, std::move(list), weak_this));
          },
          base::Unretained(workspace_store_), connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void WorkspaceApiHandler::HandleImportWorkspaces(int connection_id,
                                                  const std::string& body) {
  auto parsed = base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_list()) {
    base::DictValue error;
    error.Set("code", "INVALID_REQUEST");
    error.Set("message", "Request body must be a JSON array of workspaces");
    base::DictValue resp;
    resp.Set("success", false);
    resp.Set("error", std::move(error));
    SendJson(connection_id, net::HTTP_BAD_REQUEST,
             base::Value(std::move(resp)));
    return;
  }

  auto items = std::move(*parsed).TakeList();
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
           [](WorkspaceStore* store, base::ListValue items,
             int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<WorkspaceApiHandler> weak_this) {
            int imported = 0;
            int skipped = 0;
            std::vector<std::string> errors;

            for (auto& item : items) {
              if (!item.is_dict()) {
                skipped++;
                continue;
              }
              base::DictValue dict(std::move(item.GetDict()));
              std::string* name = dict.FindString("name");
              if (!name || name->empty()) {
                skipped++;
                continue;
              }
              Workspace ws = Workspace::FromDict(dict);
              ws.id = Workspace::GenerateId();
              store->CreateWorkspace(std::move(ws));
              imported++;
            }

            base::DictValue data;
            data.Set("imported", imported);
            data.Set("skipped", skipped);
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](int connection_id, base::DictValue data,
                       base::WeakPtr<WorkspaceApiHandler> weak_this) {
                      if (weak_this) {
                        base::DictValue result;
                        result.Set("success", true);
                        result.Set("data", std::move(data));
                        weak_this->SendJson(
                            connection_id, net::HTTP_OK,
                            base::Value(std::move(result)));
                      }
                    },
                    connection_id, std::move(data), weak_this));
          },
          base::Unretained(workspace_store_), std::move(items),
          connection_id, std::move(api_runner), std::move(weak_this)));
}

}  // namespace purecloak
