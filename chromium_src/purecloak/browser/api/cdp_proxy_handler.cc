// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/api/cdp_proxy_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

namespace {

constexpr net::NetworkTrafficAnnotationTag kCdpProxyTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("purecloak_cdp_proxy", R"(
      semantics {
        sender: "PureCloak CDP Proxy"
        description: "Proxies CDP WebSocket traffic to workspace subprocesses"
        trigger: "API client connects to CDP WebSocket endpoint"
        data: "CDP protocol frames"
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
      })");

// WebSocket key used for the upgrade handshake to the CDP target.
const char kWebSocketKey[] = "dGhlIHNhbXBsZSBub25jZQ==";

}  // namespace

CDPProxyHandler::CDPProxyHandler(RunningWorkspaceManager* manager)
    : workspace_manager_(manager) {
  DCHECK(workspace_manager_);
}

CDPProxyHandler::~CDPProxyHandler() {
  // All sessions will be destroyed automatically via unique_ptr.
}

void CDPProxyHandler::HandleWebSocketUpgrade(
    int connection_id,
    const std::string& ws_id,
    const std::string& subpath,
    const net::HttpServerRequestInfo& info,
    net::HttpServer::Delegate* delegate) {
  // Accept the WebSocket upgrade on the server side.
  auto* session = CreateSession(connection_id);
  if (!session) {
    LOG(ERROR) << "Failed to create proxy session for connection "
               << connection_id;
    return;
  }
  session->subpath = subpath;

  // Look up the workspace CDP port on the UI thread.
  auto api_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_this = weak_factory_.GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RunningWorkspaceManager* manager, std::string ws_id,
             int connection_id,
             scoped_refptr<base::SequencedTaskRunner> api_runner,
             base::WeakPtr<CDPProxyHandler> weak_this) {
            auto* running = manager->Get(ws_id);
            if (!running || running->status !=
                                RunningWorkspace::Status::kRunning) {
              LOG(ERROR) << "Workspace " << ws_id
                         << " is not running for CDP proxy";
              api_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(&CDPProxyHandler::OnClientDisconnect,
                                 weak_this, connection_id));
              return;
            }

            int cdp_port = running->cdp_port;
            api_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&CDPProxyHandler::ConnectToTarget,
                               weak_this, connection_id,
                               "127.0.0.1", cdp_port));
          },
          base::Unretained(workspace_manager_), ws_id, connection_id,
          std::move(api_runner), std::move(weak_this)));
}

void CDPProxyHandler::OnWebSocketMessage(int connection_id,
                                          std::string data) {
  auto* session = GetSession(connection_id);
  if (!session || !session->handshake_complete) {
    LOG(WARNING) << "WebSocket message before handshake complete for "
                 << connection_id;
    return;
  }
  ForwardToTarget(connection_id, std::move(data));
}

void CDPProxyHandler::OnClientDisconnect(int connection_id) {
  DestroySession(connection_id);
}

// --- Session Management ---

CDPProxyHandler::ProxySession* CDPProxyHandler::CreateSession(
    int connection_id) {
  auto session = std::make_unique<ProxySession>();
  session->client_connection_id = connection_id;
  session->read_buffer.resize(4096);
  auto* ptr = session.get();
  sessions_[connection_id] = std::move(session);
  return ptr;
}

CDPProxyHandler::ProxySession* CDPProxyHandler::GetSession(
    int connection_id) {
  auto it = sessions_.find(connection_id);
  return it != sessions_.end() ? it->second.get() : nullptr;
}

void CDPProxyHandler::DestroySession(int connection_id) {
  auto it = sessions_.find(connection_id);
  if (it != sessions_.end()) {
    if (server_) {
      server_->Close(connection_id);
    }
    sessions_.erase(it);
  }
}

// --- Target Connection ---

void CDPProxyHandler::ConnectToTarget(ProxySession* session,
                                       const std::string& host,
                                       int port) {
  // If session was destroyed while we were looking up the port, bail out.
  // (this is handled by checking if session still exists)
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      // We're on the API thread, no sequence checker for the handler itself
  );

  auto socket = std::make_unique<net::TCPClientSocket>(
      net::AddressList(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port)),
      nullptr, nullptr, nullptr, kCdpProxyTrafficAnnotation);

  // Store the socket and connect.
  session->target_socket = std::move(socket);

  int result = session->target_socket->Connect(
      base::BindOnce(&CDPProxyHandler::OnTargetConnected,
                     weak_factory_.GetWeakPtr(), session->client_connection_id));

  if (result == net::ERR_IO_PENDING) {
    // Connection will complete asynchronously.
    return;
  }

  // Connection completed synchronously (or failed).
  OnTargetConnected(session->client_connection_id, result);
}

void CDPProxyHandler::OnTargetConnected(int connection_id, int result) {
  auto* session = GetSession(connection_id);
  if (!session) {
    return;
  }

  if (result != net::OK) {
    LOG(ERROR) << "Failed to connect to CDP target: "
               << net::ErrorToString(result);
    DestroySession(connection_id);
    return;
  }

  // Connection established. Send WebSocket upgrade handshake.
  SendHandshake(session);
}

void CDPProxyHandler::SendHandshake(ProxySession* session) {
  // Build a WebSocket upgrade request manually.
  std::string handshake = base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: 127.0.0.1:%s\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n",
      session->subpath.empty() ? "/devtools/browser" : session->subpath.c_str(),
      "",  // port is embedded in Host
      kWebSocketKey);

  // Send the handshake as raw data.
  auto write_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(handshake), handshake.size());

  // TODO: Implement proper async write. For now, use a simplified write.
  int write_result = session->target_socket->Write(
      write_buffer.get(), write_buffer->BytesRemaining(),
      base::BindOnce([](int result) {
        if (result < 0) {
          LOG(ERROR) << "Failed to send WebSocket handshake: "
                     << net::ErrorToString(result);
        }
      }));

  if (write_result < 0 && write_result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Write failed for handshake: "
               << net::ErrorToString(write_result);
    DestroySession(session->client_connection_id);
    return;
  }

  // Start reading the handshake response.
  StartTargetRead(session);
}

void CDPProxyHandler::StartTargetRead(ProxySession* session) {
  auto* buffer_data = session->read_buffer.data();
  auto buffer_size = session->read_buffer.size();

  int read_result = session->target_socket->Read(
      buffer_data, buffer_size,
      base::BindOnce(&CDPProxyHandler::OnTargetRead,
                     weak_factory_.GetWeakPtr(),
                     session->client_connection_id));

  if (read_result == net::ERR_IO_PENDING) {
    // Read will complete asynchronously.
    return;
  }

  // Read completed synchronously.
  OnTargetRead(session->client_connection_id, read_result);
}

void CDPProxyHandler::OnTargetRead(int connection_id, int result) {
  auto* session = GetSession(connection_id);
  if (!session) {
    return;
  }

  if (result <= 0) {
    // Connection closed or error.
    if (result < 0) {
      LOG(ERROR) << "CDP target read error: "
                 << net::ErrorToString(result);
    }
    DestroySession(connection_id);
    return;
  }

  std::string data(session->read_buffer.data(), result);

  if (!session->handshake_complete) {
    // Check if we've received the HTTP 101 response.
    if (data.find("101 Switching Protocols") != std::string::npos) {
      session->handshake_complete = true;
      // Strip the HTTP response headers.
      auto header_end = data.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        data = data.substr(header_end + 4);
      } else {
        data.clear();
      }
      // If there's leftover data after the headers, forward it.
      if (!data.empty()) {
        ForwardToClient(connection_id, data);
      }
      // Start reading more from target.
      StartTargetRead(session);
    } else if (data.find("HTTP/1.1") != std::string::npos &&
               data.find("101") == std::string::npos) {
      // Handshake failed.
      LOG(ERROR) << "WebSocket handshake failed: " << data;
      DestroySession(connection_id);
    } else {
      // Partial header, read more.
      StartTargetRead(session);
    }
    return;
  }

  // Handshake complete, forward data as WebSocket frame payload.
  ForwardToClient(connection_id, data);
}

void CDPProxyHandler::ForwardToClient(int connection_id,
                                       const std::string& data) {
  if (!server_) {
    LOG(ERROR) << "ForwardToClient: server_ is null";
    return;
  }
  server_->SendOverWebSocket(connection_id, data);
  // Continue reading from target.
  auto* session = GetSession(connection_id);
  if (session) {
    StartTargetRead(session);
  }
}

void CDPProxyHandler::ForwardToTarget(int connection_id,
                                       const std::string& data) {
  auto* session = GetSession(connection_id);
  if (!session || !session->target_socket) {
    return;
  }

  // Send the WebSocket frame payload directly to the target.
  // For CDP, the client sends masked WebSocket frames. Since we're proxying,
  // we receive the unmasked payload from the server's SendOverWebSocket
  // callback (OnWebSocketMessage gives us the unmasked data). We forward it
  // as-is to the target via raw TCP.
  auto write_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(data), data.size());

  int write_result = session->target_socket->Write(
      write_buffer.get(), write_buffer->BytesRemaining(),
      base::BindOnce(
          [](int connection_id, int result) {
            if (result < 0) {
              LOG(ERROR) << "Target write error for connection "
                         << connection_id << ": " << net::ErrorToString(result);
            }
          },
          connection_id));

  if (write_result < 0 && write_result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Target write failed: " << net::ErrorToString(write_result);
    DestroySession(connection_id);
  }
}

}  // namespace purecloak
