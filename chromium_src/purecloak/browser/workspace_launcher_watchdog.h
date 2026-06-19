// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_WATCHDOG_H_
#define PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_WATCHDOG_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "purecloak/browser/workspace_launcher.h"

namespace purecloak {

class RunningWorkspaceManager;

// Periodically checks the health of running workspace subprocesses.
// Every 5 seconds, checks each tracked process and notifies the manager
// if a process has crashed or exited.
class WorkspaceLauncherWatchdog {
 public:
  explicit WorkspaceLauncherWatchdog(RunningWorkspaceManager* manager);
  ~WorkspaceLauncherWatchdog();

  // Start monitoring a workspace subprocess.
  void StartWatching(RunningWorkspace* ws);

  // Stop monitoring a workspace.
  void StopWatching(const std::string& ws_id);

  // Stop all monitoring.
  void StopAll();

 private:
  // Called every |kPollInterval| to check all tracked processes.
  void OnPoll();

  // Check a single process for health.
  void CheckProcess(const std::string& ws_id);

  static constexpr base::TimeDelta kPollInterval = base::Seconds(5);

  raw_ptr<RunningWorkspaceManager> manager_;

  // Set of workspace IDs currently being monitored.
  std::map<std::string, bool> watched_;

  base::RepeatingTimer poll_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_WATCHDOG_H_
