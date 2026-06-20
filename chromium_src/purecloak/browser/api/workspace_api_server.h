// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_API_WORKSPACE_API_SERVER_H_
#define PURECLOAK_BROWSER_API_WORKSPACE_API_SERVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "base/values.h"

namespace net {
class HttpServer;
}  // namespace net

namespace purecloak {

class WorkspaceStore;
class RunningWorkspaceManager;
class WorkspaceApiHandler;

// Manages the PureCloak REST API HTTP server lifecycle.
// Runs net::HttpServer on a dedicated background thread.
class WorkspaceApiServer {
 public:
  WorkspaceApiServer(WorkspaceStore* store,
                     RunningWorkspaceManager* manager);
  ~WorkspaceApiServer();

  // Start the server on |port|. Returns true on success.
  bool Start(int port);

  // Stop the server and join the background thread.
  void Stop();

  // Get the listening port (0 if not started).
  int port() const { return port_; }

 private:
  raw_ptr<WorkspaceStore> workspace_store_;
  raw_ptr<RunningWorkspaceManager> workspace_manager_;

  std::unique_ptr<WorkspaceApiHandler> handler_;
  std::unique_ptr<net::HttpServer> server_;
  std::unique_ptr<base::Thread> thread_;
  int port_ = 0;
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_API_WORKSPACE_API_SERVER_H_
