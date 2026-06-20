// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/api/workspace_api_server.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/socket/tcp_server_socket.h"
#include "purecloak/browser/api/workspace_api_handler.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

namespace {

// Traffic annotation for internal PureCloak API traffic.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("purecloak_api", R"(
      semantics {
        sender: "PureCloak API"
        description: "Internal REST API for workspace management"
        trigger: "External tool connects to the PureCloak API port"
        data: "Workspace configuration and status data"
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
      })");

}  // namespace

WorkspaceApiServer::WorkspaceApiServer(
    WorkspaceStore* store,
    RunningWorkspaceManager* manager)
    : workspace_store_(store), workspace_manager_(manager) {
  DCHECK(workspace_store_);
  DCHECK(workspace_manager_);
}

WorkspaceApiServer::~WorkspaceApiServer() {
  Stop();
}

bool WorkspaceApiServer::Start(int port) {
  if (server_) {
    LOG(WARNING) << "PureCloak API server already running on port " << port_;
    return false;
  }

  // Create background thread for the API server.
  thread_ = std::make_unique<base::Thread>("PureCloakAPI");
  if (!thread_->Start()) {
    LOG(ERROR) << "Failed to start PureCloak API thread";
    return false;
  }

  // Create handler on the current thread. The handler will be moved to and
  // live on the API thread after server creation.
  handler_ = std::make_unique<WorkspaceApiHandler>(
      workspace_store_, workspace_manager_);

  // Create the server on the API thread synchronously so we know the result.
  base::WaitableEvent done;
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<WorkspaceApiHandler> handler, int port,
             std::unique_ptr<net::HttpServer>* out_server,
             std::unique_ptr<WorkspaceApiHandler>* out_handler,
             int* out_port, base::WaitableEvent* done) {
            // Create a TCP server socket bound to 127.0.0.1.
            auto socket = std::make_unique<net::TCPServerSocket>(
                nullptr, net::NetLogSource());
            int result = socket->ListenWithAddressAndPort(
                "127.0.0.1", port, /*backlog=*/10);
            if (result != net::OK) {
              LOG(ERROR) << "Failed to bind PureCloak API to port " << port
                         << " (error: " << net::ErrorToString(result) << ")";
              *out_port = 0;
              done->Signal();
              return;
            }

            // Create the HttpServer with the socket and handler delegate.
            auto server = std::make_unique<net::HttpServer>(
                std::move(socket), handler.get());

            // Give the handler a pointer to the server for sending responses.
            handler->set_server(server.get());

            *out_server = std::move(server);
            *out_handler = std::move(handler);
            *out_port = port;
            done->Signal();
          },
          std::move(handler_), port, &server_, &handler_, &port_, &done));
  done.Wait();

  if (port_ == 0) {
    // Server creation failed. Clean up thread.
    thread_->Stop();
    thread_.reset();
    return false;
  }

  LOG(INFO) << "PureCloak API server started on 127.0.0.1:" << port_;
  return true;
}

void WorkspaceApiServer::Stop() {
  if (!thread_) {
    return;
  }

  // Destroy server and handler on the API thread.
  base::WaitableEvent done;
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<net::HttpServer>* server,
             std::unique_ptr<WorkspaceApiHandler>* handler,
             base::WaitableEvent* done) {
            handler->reset();
            server->reset();
            done->Signal();
          },
          &server_, &handler_, &done));
  done.Wait();

  thread_->Stop();
  thread_.reset();
  port_ = 0;

  LOG(INFO) << "PureCloak API server stopped";
}

}  // namespace purecloak
