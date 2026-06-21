// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/workspace_launcher.h"

#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "purecloak/browser/workspace_launcher_watchdog.h"
#include "purecloak/browser/workspace_profile_applier.h"

namespace purecloak {

namespace {

// CDP port range: 9333-9352 (20-slot range, per design spec).
constexpr int kMinCdpPort = 9333;
constexpr int kMaxCdpPort = 9352;
constexpr int kMaxConcurrentWorkspaces = 10;

// Key for SupportsUserData storage.
const char kRunningWorkspaceManagerKey[] = "purecloak.RunningWorkspaceManager";

// Find an available port in the range [kMinCdpPort, kMaxCdpPort] by trying
// to bind a temporary listening socket.
int FindAvailablePort() {
  for (int port = kMinCdpPort; port <= kMaxCdpPort; ++port) {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      continue;
    }

    // Set SO_REUSEADDR so we can detect if our own process is using it.
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))
            == 0) {
      close(sock);
      return port;  // Port is available.
    }
    close(sock);
  }
  return 0;  // No port available.
}

}  // namespace

// --- RunningWorkspace ---

// static
const char* RunningWorkspace::StatusToString(Status status) {
  switch (status) {
    case Status::kStarting:
      return "starting";
    case Status::kRunning:
      return "running";
    case Status::kStopped:
      return "stopped";
    case Status::kCrashed:
      return "crashed";
  }
  return "unknown";
}

// static
RunningWorkspace::Status RunningWorkspace::StringToStatus(
    const std::string& str) {
  if (str == "starting") return Status::kStarting;
  if (str == "running") return Status::kRunning;
  if (str == "crashed") return Status::kCrashed;
  return Status::kStopped;
}

base::DictValue RunningWorkspace::ToDict() const {
  base::DictValue dict;
  dict.Set("workspace_id", workspace_id);
  dict.Set("status", StatusToString(status));
  dict.Set("cdp_port", cdp_port);
  dict.Set("cdp_url", cdp_url);
  if (process.IsValid()) {
    dict.Set("pid", static_cast<int>(process.Pid()));
  }
  dict.Set("uptime_seconds", static_cast<int>(Uptime().InSeconds()));
  dict.Set("launched_at",
           static_cast<double>(launched_at.ToDeltaSinceWindowsEpoch()
                                   .InMicrosecondsF()));
  return dict;
}

base::TimeDelta RunningWorkspace::Uptime() const {
  if (launched_at.is_null()) {
    return base::TimeDelta();
  }
  return base::Time::Now() - launched_at;
}

// --- RunningWorkspaceManager ---

// static
RunningWorkspaceManager* RunningWorkspaceManager::GetOrCreate(
    content::BrowserContext* context) {
  if (!context) {
    return nullptr;
  }
  auto* data = static_cast<RunningWorkspaceManager*>(
      context->GetUserData(kRunningWorkspaceManagerKey));
  if (!data) {
    auto manager = std::unique_ptr<RunningWorkspaceManager>(
        new RunningWorkspaceManager());
    data = manager.get();
    context->SetUserData(kRunningWorkspaceManagerKey, std::move(manager));
  }
  return data;
}

RunningWorkspaceManager::RunningWorkspaceManager() {
  watchdog_ = std::make_unique<WorkspaceLauncherWatchdog>(this);
}

RunningWorkspaceManager::~RunningWorkspaceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopAll();
}

void RunningWorkspaceManager::Start(const Workspace& ws,
                                     StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if already running.
  auto it = workspaces_.find(ws.id);
  if (it != workspaces_.end() &&
      it->second->status != RunningWorkspace::Status::kStopped &&
      it->second->status != RunningWorkspace::Status::kCrashed) {
    std::move(callback).Run(true, it->second->ToDict());
    return;
  }

  // Check concurrent limit.
  if (workspaces_.size() >= kMaxConcurrentWorkspaces) {
    base::DictValue error;
    error.Set("success", false);
    error.Set("error", "Maximum concurrent workspaces reached");
    std::move(callback).Run(false, std::move(error));
    return;
  }

  // Find an available port.
  int cdp_port = FindAvailablePort();
  if (cdp_port == 0) {
    base::DictValue error;
    error.Set("success", false);
    error.Set("error", "No available CDP port");
    std::move(callback).Run(false, std::move(error));
    return;
  }

  // Create user-data-dir.
  base::FilePath user_data_dir = GetWorkspaceBaseDir()
                                     .AppendASCII("ws_" + ws.id);

  // Prepare user-data-dir on a blocking task runner.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& dir,
             const std::string& locale) -> bool {
            // Create directory structure.
            base::CreateDirectory(dir);
            base::FilePath default_dir = dir.AppendASCII("Default");
            base::CreateDirectory(default_dir);

            // Write Preferences file with workspace settings.
            base::DictValue prefs;
            if (!locale.empty()) {
              base::DictValue intl;
              intl.Set("accept_languages", locale);
              prefs.Set("intl", std::move(intl));
            }

            std::string prefs_json;
            base::JSONWriter::Write(prefs, &prefs_json);
            base::WriteFile(default_dir.AppendASCII("Preferences"),
                            prefs_json);
            return true;
          },
          user_data_dir, ws.locale),
      base::BindOnce(
          [](base::WeakPtr<RunningWorkspaceManager> self,
             const Workspace ws,
             const base::FilePath user_data_dir, int cdp_port,
             StartCallback callback, bool success) {
            if (!self) {
              std::move(callback).Run(false, base::DictValue());
              return;
            }
            self->ContinueStart(std::move(ws),
                                user_data_dir, cdp_port, std::move(callback));
          },
          weak_factory_.GetWeakPtr(), ws, user_data_dir, cdp_port,
          std::move(callback)));
}

void RunningWorkspaceManager::ContinueStart(
    const Workspace& ws,
    const base::FilePath& user_data_dir,
    int cdp_port,
    StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Build the command line.
  base::CommandLine cmd_line = BuildCommandLine(ws, user_data_dir,
                                                  cdp_port);

  // Launch the subprocess.
  base::LaunchOptions options;
  options.new_process_group = true;

  base::Process process = base::LaunchProcess(cmd_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch workspace subprocess for " << ws.id;
    base::DictValue error;
    error.Set("success", false);
    error.Set("error", "Failed to launch subprocess");
    std::move(callback).Run(false, std::move(error));
    return;
  }

  // Create the RunningWorkspace record.
  auto running = std::make_unique<RunningWorkspace>();
  running->workspace_id = ws.id;
  running->type = ws.type;
  running->process = std::move(process);
  running->user_data_dir = user_data_dir;
  running->cdp_port = cdp_port;
  running->cdp_url =
      base::StringPrintf("http://127.0.0.1:%d", cdp_port);
  running->status = RunningWorkspace::Status::kStarting;
  running->launched_at = base::Time::Now();

  // Transition to kRunning after a short delay (give the process time to
  // initialize the CDP endpoint).
  RunningWorkspace* ws_ptr = running.get();
  workspaces_[ws.id] = std::move(running);

  // Start watchdog monitoring.
  watchdog_->StartWatching(ws_ptr);

  // Mark as running.
  ws_ptr->status = RunningWorkspace::Status::kRunning;
  NotifyObserversWorkspaceLaunched(*ws_ptr);

  // Apply CDP settings to the running workspace.
  {
    auto applier = std::make_unique<WorkspaceProfileApplier>();
    applier->Apply(*ws_ptr, {ws},
                    base::BindOnce([](bool success) {
                      if (!success) {
                        LOG(WARNING) << "CDP workspace setting application failed";
                      }
                    }));
    appliers_[ws_ptr->workspace_id] = std::move(applier);
  }

  std::move(callback).Run(true, ws_ptr->ToDict());
}

void RunningWorkspaceManager::Stop(const std::string& ws_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = workspaces_.find(ws_id);
  if (it == workspaces_.end()) {
    return;
  }

  RunningWorkspace* ws = it->second.get();

  // Stop watchdog monitoring.
  watchdog_->StopWatching(ws_id);

  // Terminate the process (async wait — never block the UI thread).
  if (ws->process.IsValid()) {
    ws->process.Terminate(0, /*wait=*/false);
  }

  // Clean up user-data-dir (configurable; default = delete).
  if (!ws->user_data_dir.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](const base::FilePath& dir) {
              base::DeletePathRecursively(dir);
            },
            ws->user_data_dir));
  }

  ws->status = RunningWorkspace::Status::kStopped;
  NotifyObserversWorkspaceStopped(ws_id);

  appliers_.erase(ws_id);
  workspaces_.erase(it);
}

void RunningWorkspaceManager::StopAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop watchdog.
  if (watchdog_) {
    watchdog_->StopAll();
  }

  // Terminate all processes.
  for (auto& [id, ws] : workspaces_) {
    if (ws->process.IsValid()) {
      ws->process.Terminate(0, /*wait=*/false);
      ws->status = RunningWorkspace::Status::kStopped;
    }

    // Clean up user-data-dir.
    if (!ws->user_data_dir.empty()) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(
              [](const base::FilePath& dir) {
                base::DeletePathRecursively(dir);
              },
              ws->user_data_dir));
    }
  }

  appliers_.clear();
  workspaces_.clear();
}

RunningWorkspace* RunningWorkspaceManager::Get(const std::string& ws_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(ws_id);
  if (it == workspaces_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<RunningWorkspace*> RunningWorkspaceManager::GetAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<RunningWorkspace*> result;
  result.reserve(workspaces_.size());
  for (auto& [id, ws] : workspaces_) {
    result.push_back(ws.get());
  }
  return result;
}

bool RunningWorkspaceManager::IsRunning(const std::string& ws_id) const {
  auto it = workspaces_.find(ws_id);
  if (it == workspaces_.end()) {
    return false;
  }
  return it->second->status == RunningWorkspace::Status::kRunning ||
         it->second->status == RunningWorkspace::Status::kStarting;
}

size_t RunningWorkspaceManager::RunningCount() const {
  size_t count = 0;
  for (const auto& [id, ws] : workspaces_) {
    if (ws->status == RunningWorkspace::Status::kRunning ||
        ws->status == RunningWorkspace::Status::kStarting) {
      ++count;
    }
  }
  return count;
}

void RunningWorkspaceManager::AddObserver(
    RunningWorkspaceObserver* observer) {
  observers_.AddObserver(observer);
}

void RunningWorkspaceManager::RemoveObserver(
    RunningWorkspaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RunningWorkspaceManager::NotifyStatusChanged(
    const std::string& ws_id,
    RunningWorkspace::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = workspaces_.find(ws_id);
  if (it != workspaces_.end()) {
    it->second->status = status;
  }

  base::DictValue details;
  if (it != workspaces_.end()) {
    details = it->second->ToDict();
  }
  details.Set("workspace_id", ws_id);
  details.Set("status", RunningWorkspace::StatusToString(status));

  for (auto& observer : observers_) {
    observer.OnWorkspaceStatusChanged(ws_id, status, details);
  }
}

base::DictValue RunningWorkspaceManager::GetStatusDict(
    const std::string& ws_id) const {
  auto it = workspaces_.find(ws_id);
  if (it == workspaces_.end()) {
    base::DictValue stopped;
    stopped.Set("workspace_id", ws_id);
    stopped.Set("status", "stopped");
    return stopped;
  }
  return it->second->ToDict();
}

void RunningWorkspaceManager::NotifyObserversWorkspaceLaunched(
    const RunningWorkspace& ws) {
  for (auto& observer : observers_) {
    observer.OnWorkspaceLaunched(ws);
  }
}

void RunningWorkspaceManager::NotifyObserversWorkspaceStopped(
    const std::string& ws_id) {
  for (auto& observer : observers_) {
    observer.OnWorkspaceStopped(ws_id);
  }
}

base::FilePath RunningWorkspaceManager::GetWorkspaceBaseDir() const {
  base::FilePath tmp_dir;
  if (!base::PathService::Get(base::DIR_TEMP, &tmp_dir)) {
    tmp_dir = base::FilePath("/tmp");
  }
  return tmp_dir.AppendASCII("purecloak");
}

base::FilePath RunningWorkspaceManager::GetChromeExecutablePath() const {
  base::FilePath exe_path;
  if (base::PathService::Get(base::FILE_EXE, &exe_path)) {
    return exe_path;
  }
  // Fallback: assume "purecloak" is in PATH.
  return base::FilePath("purecloak");
}

base::CommandLine RunningWorkspaceManager::BuildCommandLine(
    const Workspace& ws,
    const base::FilePath& user_data_dir,
    int cdp_port) const {
  base::CommandLine cmd(GetChromeExecutablePath());

  // Core flags for isolation.
  cmd.AppendSwitchPath("user-data-dir", user_data_dir);
  cmd.AppendSwitchASCII("remote-debugging-port",
                        base::NumberToString(cdp_port));
  cmd.AppendSwitch("no-first-run");
  cmd.AppendSwitch("no-default-browser-check");
  cmd.AppendSwitchASCII("disable-features", "Translate");

  // Workspace launch-time flags.
  if (!ws.user_agent.empty()) {
    cmd.AppendSwitchASCII("user-agent", ws.user_agent);
  }
  if (ws.screen_width > 0 && ws.screen_height > 0) {
    cmd.AppendSwitchASCII(
        "window-size",
        base::StringPrintf("%d,%d", ws.screen_width,
                           ws.screen_height));
  }
  if (!ws.proxy.empty()) {
    cmd.AppendSwitchASCII("proxy-server", ws.proxy);
  }
  if (!ws.locale.empty()) {
    cmd.AppendSwitchASCII("accept-lang", ws.locale);
  }
  if (ws.headless) {
    cmd.AppendSwitchASCII("headless", "new");
  }
  // WebRTC IP handling: disable non-proxied UDP to prevent IP leaks.
  cmd.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                        "disable_non_proxied_udp");

  // Extra launch args from workspace.
  for (const auto& arg : ws.launch_args) {
    cmd.AppendArg(arg);
  }

  // URL argument.
  cmd.AppendArg("about:blank");

  return cmd;
}

}  // namespace purecloak
