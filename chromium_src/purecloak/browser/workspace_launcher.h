// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_H_
#define PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/values.h"
#include "purecloak/browser/profiles/workspace.h"

namespace content {
class BrowserContext;
}

namespace purecloak {

class RunningWorkspaceManager;
class WorkspaceLauncherWatchdog;
class WorkspaceProfileApplier;

// Represents a single running workspace subprocess.
struct RunningWorkspace {
  enum class Status { kStarting, kRunning, kStopped, kCrashed };

  std::string workspace_id;
  Workspace::Type type = Workspace::Type::kNormal;
  base::Process process;
  base::FilePath user_data_dir;
  int cdp_port = 0;
  std::string cdp_url;
  Status status = Status::kStarting;
  base::Time launched_at;

  static const char* StatusToString(Status status);
  static Status StringToStatus(const std::string& str);
  base::DictValue ToDict() const;

  // Time since launch.
  base::TimeDelta Uptime() const;
};

// Observer interface for workspace status changes.
class RunningWorkspaceObserver : public base::CheckedObserver {
 public:
  // Called when a workspace status changes (started, stopped, crashed).
  virtual void OnWorkspaceStatusChanged(const std::string& workspace_id,
                                        RunningWorkspace::Status status,
                                        const base::DictValue& details) {}

  // Called when a workspace is launched.
  virtual void OnWorkspaceLaunched(const RunningWorkspace& ws) {}

  // Called when a workspace is stopped.
  virtual void OnWorkspaceStopped(const std::string& workspace_id) {}

 protected:
  RunningWorkspaceObserver() = default;
  ~RunningWorkspaceObserver() override = default;
};

// Manages running workspace subprocesses.
//
// Stored as SupportsUserData::Data on the BrowserContext so that each
// browser profile gets its own manager instance.
//
// Lifecycle:
//   1. Start() launches a Chrome child process with isolated --user-data-dir
//   2. Watchdog monitors process health every 5 seconds
//   3. Stop() terminates the process and cleans up
//   4. StopAll() on manager destruction / browser exit
class RunningWorkspaceManager : public base::SupportsUserData::Data {
 public:
  ~RunningWorkspaceManager() override;

  // Get or create the manager for a given BrowserContext.
  static RunningWorkspaceManager* GetOrCreate(content::BrowserContext* context);

  // Launch a workspace as an independent Chrome subprocess.
  // |callback| is called with the CDP URL on success, or empty on failure.
  using StartCallback =
      base::OnceCallback<void(bool success, const base::DictValue& result)>;

  void Start(const Workspace& ws,
             StartCallback callback);

  // Stop a running workspace by ID.
  void Stop(const std::string& ws_id);

  // Stop all running workspaces. Called on browser exit.
  void StopAll();

  // Queries.
  RunningWorkspace* Get(const std::string& ws_id);
  std::vector<RunningWorkspace*> GetAll();
  bool IsRunning(const std::string& ws_id) const;
  size_t RunningCount() const;

  // Observers.
  void AddObserver(RunningWorkspaceObserver* observer);
  void RemoveObserver(RunningWorkspaceObserver* observer);

  // Notify observers of a status change (called by Watchdog).
  void NotifyStatusChanged(const std::string& ws_id,
                           RunningWorkspace::Status status);

  // Get workspace status as a dictionary for WebUI responses.
  base::DictValue GetStatusDict(const std::string& ws_id) const;

 private:
  friend class WorkspaceLauncherWatchdog;

  RunningWorkspaceManager();

  // Builds the command line for the PureCloak subprocess.
  base::CommandLine BuildCommandLine(
      const Workspace& ws,
      const base::FilePath& user_data_dir,
      int cdp_port) const;

  // Gets the PureCloak executable path (same binary as the parent process).
  base::FilePath GetChromeExecutablePath() const;

  // Notifies all observers.
  void NotifyObserversWorkspaceLaunched(const RunningWorkspace& ws);
  void NotifyObserversWorkspaceStopped(const std::string& ws_id);

  // Gets the base temp directory for PureCloak workspaces.
  base::FilePath GetWorkspaceBaseDir() const;

  // Continues the start sequence after user-data-dir is prepared.
  void ContinueStart(const Workspace& ws,
                     const base::FilePath& user_data_dir,
                     int cdp_port,
                     StartCallback callback);

  std::map<std::string, std::unique_ptr<RunningWorkspace>> workspaces_;
  std::map<std::string, std::unique_ptr<WorkspaceProfileApplier>> appliers_;
  base::ObserverList<RunningWorkspaceObserver> observers_;
  std::unique_ptr<WorkspaceLauncherWatchdog> watchdog_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RunningWorkspaceManager> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_WORKSPACE_LAUNCHER_H_
