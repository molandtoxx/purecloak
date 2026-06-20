// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_API_CDP_PROXY_HANDLER_H_
#define PURECLOAK_BROWSER_API_CDP_PROXY_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace net {
class HttpServer;
class HttpServerRequestInfo;
class StreamSocket;
class TCPClientSocket;
}  // namespace net

namespace purecloak {

class RunningWorkspaceManager;

// Manages CDP WebSocket proxy sessions.
//
// Each session proxies WebSocket traffic between an API client and a running
// workspace's CDP port. The flow is:
//   1. API client sends a WebSocket upgrade request to
//      /api/workspaces/{ws_id}/cdp/{subpath}
//   2. CDPProxyHandler accepts the upgrade and connects via raw TCP to the
//      workspace's CDP endpoint at 127.0.0.1:<cdp_port>
//   3. It performs the WebSocket handshake with the target
//   4. Frames are relayed bidirectionally until either side disconnects
class CDPProxyHandler {
 public:
  explicit CDPProxyHandler(RunningWorkspaceManager* manager);
  CDPProxyHandler(const CDPProxyHandler&) = delete;
  CDPProxyHandler& operator=(const CDPProxyHandler&) = delete;
  ~CDPProxyHandler();

  // Set the HttpServer pointer for sending WS frames.
  void set_server(net::HttpServer* server) { server_ = server; }

  // Called from WorkspaceApiHandler when a WS upgrade request arrives.
  // |delegate| is the HttpServer::Delegate that accepted the original upgrade.
  void HandleWebSocketUpgrade(
      int connection_id,
      const std::string& ws_id,
      const std::string& subpath,
      const net::HttpServerRequestInfo& info,
      net::HttpServer::Delegate* delegate);

  // Forward a WebSocket message from the client to the CDP target.
  void OnWebSocketMessage(int connection_id, std::string data);

  // Clean up when the client disconnects.
  void OnClientDisconnect(int connection_id);

 private:
  struct ProxySession {
    int client_connection_id = 0;
    std::unique_ptr<net::StreamSocket> target_socket;
    std::string subpath;
    bool handshake_complete = false;
    std::vector<char> read_buffer;
    std::string partial_frame;
  };

  ProxySession* CreateSession(int connection_id);
  ProxySession* GetSession(int connection_id);
  void DestroySession(int connection_id);

  // Connect to the workspace's CDP port.
  void ConnectToTarget(ProxySession* session, const std::string& host,
                       int port);

  // Send WebSocket upgrade handshake to the CDP target.
  void SendHandshake(ProxySession* session);

  // Start reading from the target socket.
  void StartTargetRead(ProxySession* session);

  // Called when data is read from the target.
  void OnTargetRead(ProxySession* session, int result);

  // Called when the target connection completes.
  void OnTargetConnected(ProxySession* session, int result);

  // Forward data from target to client over the established WebSocket.
  void ForwardToClient(int connection_id, const std::string& data);

  // Forward data from client to target (over raw TCP).
  void ForwardToTarget(int connection_id, const std::string& data);

  raw_ptr<RunningWorkspaceManager> workspace_manager_;
  raw_ptr<net::HttpServer> server_ = nullptr;

  // Map from client connection_id to proxy session.
  std::map<int, std::unique_ptr<ProxySession>> sessions_;

  base::WeakPtrFactory<CDPProxyHandler> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_API_CDP_PROXY_HANDLER_H_
