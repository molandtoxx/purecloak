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

WorkspaceApiServer::WorkspaceApiServer(
    WorkspaceStore* store,
    RunningWorkspaceManager* manager,
    const std::string& api_token)
    : workspace_store_(store), workspace_manager_(manager),
      api_token_(api_token) {
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

  // Create background thread for the API server (needs IO message loop for
  // net::HttpServer).
  thread_ = std::make_unique<base::Thread>("PureCloakAPI");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  if (!thread_->StartWithOptions(std::move(thread_options))) {
    LOG(ERROR) << "Failed to start PureCloak API thread";
    return false;
  }

  // Create the server on the API thread synchronously so we know the result.
  // Handler is also created on the API thread so its SEQUENCE_CHECKER binds
  // to the correct sequence.
  base::WaitableEvent done;
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WorkspaceStore* store, RunningWorkspaceManager* manager,
             int port,
             std::unique_ptr<net::HttpServer>* out_server,
             std::unique_ptr<WorkspaceApiHandler>* out_handler,
             int* out_port, base::WaitableEvent* done,
             const std::string& api_token) {
            auto handler = std::make_unique<WorkspaceApiHandler>(
                store, manager, api_token);

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
          base::Unretained(workspace_store_),
          base::Unretained(workspace_manager_),
          port, &server_, &handler_, &port_, &done, api_token_));
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
