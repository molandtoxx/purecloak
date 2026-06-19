// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_WORKSPACE_PROFILE_APPLIER_H_
#define PURECLOAK_BROWSER_WORKSPACE_PROFILE_APPLIER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

class ProfileCDPInjector;
struct RunningWorkspace;

// Orchestrates profile setting application for a launched workspace.
//
// Phase 1: Launch-time settings are applied via command-line flags
//          in RunningWorkspaceManager::BuildCommandLine.
// Phase 2: Runtime settings are applied via CDP commands after the
//          subprocess is ready.
//
// The applier waits for the CDP endpoint to become available, then
// sends all runtime commands via ProfileCDPInjector.
class WorkspaceProfileApplier {
 public:
  using ApplyCallback = base::OnceCallback<void(bool success)>;

  WorkspaceProfileApplier();
  ~WorkspaceProfileApplier();

  // Apply all runtime profile settings to a running workspace.
  // |ws| provides the CDP endpoint, |profiles| provides the settings.
  // |callback| is called when all commands have been applied.
  void Apply(const RunningWorkspace& ws,
             const std::vector<Workspace>& workspaces,
             ApplyCallback callback);

 private:
  void OnPollTick();
  void OnCDPCheckResult(bool available);
  void OnCommandsSent(int fd);
  void OnHealthCheck();
  void ReconnectCDP();

  std::vector<base::DictValue> GenerateCommandBatch(
      const std::vector<Workspace>& workspaces) const;

  std::unique_ptr<ProfileCDPInjector> injector_;

  base::RepeatingTimer cdp_poll_timer_;
  base::RepeatingTimer cdp_health_timer_;
  int cdp_port_ = 0;
  int poll_retries_left_ = 0;
  bool commands_started_ = false;  // Guard against re-entrant OnCDPCheckResult.
  std::vector<std::string> pending_command_jsons_;
  ApplyCallback pending_callback_;

  // Open CDP WebSocket socket fd that must be kept alive so Chrome 151+ does
  // not remove session-scoped scripts (Page.addScriptToEvaluateOnNewDocument).
  // -1 when not connected or after error.
  int cdp_socket_ = -1;

  // Saved command JSONs for reconnection when the WebSocket drops.
  std::vector<std::string> saved_command_jsons_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WorkspaceProfileApplier> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_WORKSPACE_PROFILE_APPLIER_H_
