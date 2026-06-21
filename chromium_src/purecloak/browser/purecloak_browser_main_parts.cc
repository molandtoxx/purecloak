// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/purecloak_browser_main_parts.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "purecloak/browser/api/workspace_api_server.h"
#include "purecloak/common/purecloak_switches.h"
#include "purecloak/browser/profiles/store_provider.h"
#include "purecloak/browser/profiles/workspace_store.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

namespace {

// Singleton API server instance. Created once, destroyed at shutdown.
WorkspaceApiServer* g_api_server = nullptr;

constexpr int kDefaultApiPort = 9334;

}  // namespace

void StartPureCloakApiServer(Profile* profile) {
  if (g_api_server) {
    return;  // Already started.
  }

  if (!profile) {
    return;
  }

  // Determine API port from command line.
  int port = kDefaultApiPort;
  std::string port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          purecloak::switches::kPureCloakApiPort);
  if (!port_str.empty()) {
    base::StringToInt(port_str, &port);
  }

  // Get the WorkspaceStore and RunningWorkspaceManager for this profile.
  WorkspaceStore* store =
      StoreProvider::GetInstance().GetWorkspaceStore(profile);
  if (!store) {
    LOG(ERROR) << "Failed to get WorkspaceStore for API server";
    return;
  }

  RunningWorkspaceManager* manager =
      RunningWorkspaceManager::GetOrCreate(profile);
  if (!manager) {
    LOG(ERROR) << "Failed to get RunningWorkspaceManager for API server";
    return;
  }

  // Determine API token from command line.
  std::string api_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          purecloak::switches::kPureCloakApiToken);

  // Create and start the API server.
  auto* server = new WorkspaceApiServer(store, manager, api_token);
  if (server->Start(port)) {
    g_api_server = server;
    LOG(INFO) << "PureCloak REST API server started on port " << port;
  } else {
    LOG(ERROR) << "Failed to start PureCloak REST API server on port " << port;
    delete server;
  }
}

void StopPureCloakApiServer() {
  if (g_api_server) {
    g_api_server->Stop();
    delete g_api_server;
    g_api_server = nullptr;
    LOG(INFO) << "PureCloak REST API server stopped";
  }
}

}  // namespace purecloak
