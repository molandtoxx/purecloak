// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/workspace_launcher_watchdog.h"

#include <algorithm>

#include "base/logging.h"
#include "base/process/process.h"

namespace purecloak {

WorkspaceLauncherWatchdog::WorkspaceLauncherWatchdog(
    RunningWorkspaceManager* manager)
    : manager_(manager) {}

WorkspaceLauncherWatchdog::~WorkspaceLauncherWatchdog() {
  StopAll();
}

void WorkspaceLauncherWatchdog::StartWatching(RunningWorkspace* ws) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ws);

  watched_[ws->workspace_id] = true;

  // Start the timer if not already running.
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE, kPollInterval, this,
                      &WorkspaceLauncherWatchdog::OnPoll);
  }
}

void WorkspaceLauncherWatchdog::StopWatching(const std::string& ws_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watched_.erase(ws_id);

  // Stop timer if nothing left to watch.
  if (watched_.empty() && poll_timer_.IsRunning()) {
    poll_timer_.Stop();
  }
}

void WorkspaceLauncherWatchdog::StopAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watched_.clear();
  if (poll_timer_.IsRunning()) {
    poll_timer_.Stop();
  }
}

void WorkspaceLauncherWatchdog::OnPoll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Iterate over a copy because CheckProcess may modify |watched_|.
  auto watched_copy = watched_;
  for (const auto& [ws_id, active] : watched_copy) {
    if (active) {
      CheckProcess(ws_id);
    }
  }
}

void WorkspaceLauncherWatchdog::CheckProcess(const std::string& ws_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunningWorkspace* ws = manager_->Get(ws_id);
  if (!ws) {
    StopWatching(ws_id);
    return;
  }

  if (!ws->process.IsValid()) {
    LOG(WARNING) << "Workspace " << ws_id << " process invalid, marking crashed";
    // Stop watching before notifying observers, since the observer callback
    // may cause the workspace to be removed (avoid use-after-free on `ws`).
    StopWatching(ws_id);
    manager_->NotifyStatusChanged(ws_id, RunningWorkspace::Status::kCrashed);
    return;
  }

  // Non-blocking check: zero-timeout WaitForExitWithTimeout returns false
  // if the process is still running, true if it has exited.
  int exit_code = 0;
  if (!ws->process.WaitForExitWithTimeout(base::TimeDelta(), &exit_code)) {
    // Process is still running. Stay healthy.
    return;
  }

  // Save status before any observer could modify the workspace.
  bool was_intentionally_stopped =
      ws->status == RunningWorkspace::Status::kStopped;
  if (!was_intentionally_stopped) {
    LOG(WARNING) << "Workspace " << ws_id << " process exited with code "
                 << exit_code << ", marking crashed";
  }

  // Stop watching first, then notify — observers may modify workspace state.
  StopWatching(ws_id);
  if (!was_intentionally_stopped) {
    manager_->NotifyStatusChanged(ws_id, RunningWorkspace::Status::kCrashed);
  }
}

}  // namespace purecloak
