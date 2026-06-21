// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_API_WORKSPACE_API_HANDLER_H_
#define PURECLOAK_BROWSER_API_WORKSPACE_API_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "net/server/http_server.h"

namespace purecloak {

class WorkspaceStore;
class RunningWorkspaceManager;
class CDPProxyHandler;

// HTTP request handler for the PureCloak REST API.
// Dispatches HTTP requests based on path + method.
// Runs on the PureCloak API background thread.
class WorkspaceApiHandler : public net::HttpServer::Delegate {
 public:
  WorkspaceApiHandler(WorkspaceStore* store,
                      RunningWorkspaceManager* manager,
                      const std::string& api_token = "");
  WorkspaceApiHandler(const WorkspaceApiHandler&) = delete;
  WorkspaceApiHandler& operator=(const WorkspaceApiHandler&) = delete;
  ~WorkspaceApiHandler() override;

  // Set the HttpServer pointer (called after server creation on the same
  // thread). Required for sending responses.
  void set_server(net::HttpServer* server) { server_ = server; }

  // net::HttpServer::Delegate:
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

 private:
  // --- Route dispatch ---
  void DispatchRequest(int connection_id,
                       const net::HttpServerRequestInfo& info);

  // --- Response helpers (must be called on API thread) ---
  void SendJson(int connection_id, net::HttpStatusCode status,
                const base::Value& response);
  void SendError(int connection_id, net::HttpStatusCode status,
                 const std::string& code, const std::string& message);
  void SendSuccess(int connection_id, base::Value data,
                   net::HttpStatusCode status = net::HTTP_OK);

  // --- REST handlers ---
  // Each handler posts to the UI thread to access WorkspaceStore or
  // RunningWorkspaceManager, then posts back to the API thread to respond.

  // GET /api/workspaces
  void HandleGetAllWorkspaces(int connection_id);
  // POST /api/workspaces
  void HandleCreateWorkspace(int connection_id, base::DictValue body);
  // GET /api/workspaces/{id}
  void HandleGetWorkspace(int connection_id, const std::string& ws_id);
  // PUT /api/workspaces/{id}
  void HandleUpdateWorkspace(int connection_id, const std::string& ws_id,
                             base::DictValue body);
  // DELETE /api/workspaces/{id}
  void HandleDeleteWorkspace(int connection_id, const std::string& ws_id);
  // POST /api/workspaces/{id}/launch
  void HandleLaunchWorkspace(int connection_id, const std::string& ws_id);
  // POST /api/workspaces/{id}/stop
  void HandleStopWorkspace(int connection_id, const std::string& ws_id);
  // GET /api/workspaces/{id}/status
  void HandleGetWorkspaceStatus(int connection_id, const std::string& ws_id);
  // GET /api/status
  void HandleSystemStatus(int connection_id);

  // CDP proxy HTTP endpoints (e.g. /json/version, /json/list)
  void HandleCDPRequest(int connection_id, const std::string& ws_id,
                        const std::string& subpath,
                         const net::HttpServerRequestInfo& info);

  // Import/export handlers.
  void HandleExportWorkspaces(int connection_id);
  void HandleImportWorkspaces(int connection_id, const std::string& body);

  // Retry a CDP request once after a delay (Phase 3.3 reconnect).
  void RetryCDPRequest(int connection_id, const std::string& ws_id,
                        const std::string& subpath,
                        net::HttpServerRequestInfo info);

  // --- Cross-thread response helpers ---
  // These are called on the API thread after UI thread work completes.
  void RespondWithWorkspaceList(int connection_id,
                                std::vector<base::DictValue> workspaces);
  void RespondWithWorkspace(int connection_id, std::optional<base::DictValue> ws);
  void RespondWithStatus(int connection_id, bool success,
                         base::DictValue data);

  raw_ptr<WorkspaceStore> workspace_store_;
  raw_ptr<RunningWorkspaceManager> workspace_manager_;
  raw_ptr<net::HttpServer> server_ = nullptr;
  std::string api_token_;

  std::unique_ptr<CDPProxyHandler> cdp_proxy_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WorkspaceApiHandler> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_API_WORKSPACE_API_HANDLER_H_
