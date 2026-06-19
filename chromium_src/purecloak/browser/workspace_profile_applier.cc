// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/workspace_profile_applier.h"

#include <poll.h>
#include <unistd.h>

#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "purecloak/browser/workspace_launcher.h"
#include "purecloak/content/cdp_websocket_client.h"
#include "purecloak/content/profile_cdp_injector.h"

namespace purecloak {

WorkspaceProfileApplier::WorkspaceProfileApplier()
    : injector_(std::make_unique<ProfileCDPInjector>()) {}

WorkspaceProfileApplier::~WorkspaceProfileApplier() {
  if (cdp_socket_ >= 0) {
    VLOG(1) << "Closing CDP WebSocket fd=" << cdp_socket_;
    close(cdp_socket_);
    cdp_socket_ = -1;
  }
}

void WorkspaceProfileApplier::Apply(const RunningWorkspace& ws,
                                     const std::vector<Workspace>& workspaces,
                                     ApplyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (workspaces.empty()) {
    std::move(callback).Run(true);
    return;
  }

  injector_->SetCDPEndpoint(ws.cdp_url);

  auto commands = GenerateCommandBatch(workspaces);
  for (size_t i = 0; i < commands.size(); ++i) {
    commands[i].Set("id", static_cast<int>(i + 1));
    std::string json;
    base::JSONWriter::Write(commands[i], &json);
    pending_command_jsons_.push_back(std::move(json));
  }

  // Prepend Page.enable (required before Page.addScriptToEvaluateOnNewDocument
  // in Chrome 151+; without it the script is silently accepted but never runs).
  base::DictValue page_enable;
  page_enable.Set("id", 0);
  page_enable.Set("method", "Page.enable");
  page_enable.Set("params", base::DictValue());
  std::string enable_json;
  base::JSONWriter::Write(page_enable, &enable_json);
  pending_command_jsons_.insert(pending_command_jsons_.begin(),
                                std::move(enable_json));

  cdp_port_ = ws.cdp_port;
  pending_callback_ = std::move(callback);
  poll_retries_left_ = 30;

  cdp_poll_timer_.Start(
      FROM_HERE, base::Milliseconds(500),
      base::BindRepeating(&WorkspaceProfileApplier::OnPollTick,
                          base::Unretained(this)));
}

void WorkspaceProfileApplier::OnPollTick() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int port = cdp_port_;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CDPWebSocketClient::IsCDPAvailable, port),
      base::BindOnce(&WorkspaceProfileApplier::OnCDPCheckResult,
                     weak_factory_.GetWeakPtr()));
}

void WorkspaceProfileApplier::OnCDPCheckResult(bool available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Guard: multiple OnPollTick calls may race, so only handle the first
  // successful check or the first timeout failure.
  if (commands_started_)
    return;

  if (available) {
    commands_started_ = true;
    cdp_poll_timer_.Stop();
    VLOG(1) << "CDP endpoint available on port " << cdp_port_
            << ", sending " << pending_command_jsons_.size() << " commands";

    int port = cdp_port_;
    // Save commands for potential reconnection before moving them.
    saved_command_jsons_ = pending_command_jsons_;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CDPWebSocketClient::SendCommands, port,
                       std::move(pending_command_jsons_)),
        base::BindOnce(&WorkspaceProfileApplier::OnCommandsSent,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (--poll_retries_left_ <= 0) {
    commands_started_ = true;
    cdp_poll_timer_.Stop();
    LOG(ERROR) << "CDP endpoint on port " << cdp_port_
               << " not available after timeout";
    std::move(pending_callback_).Run(false);
  }
}

void WorkspaceProfileApplier::OnCommandsSent(int fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (fd >= 0) {
    cdp_socket_ = fd;
    VLOG(1) << "CDP commands sent, keeping WS fd=" << fd
            << " open (Chrome 151+ session-scoped scripts)";

    // Start health check timer to detect WebSocket disconnect.
    cdp_health_timer_.Start(
        FROM_HERE, base::Seconds(5),
        base::BindRepeating(&WorkspaceProfileApplier::OnHealthCheck,
                            base::Unretained(this)));
  } else {
    LOG(ERROR) << "Failed to send CDP commands to port " << cdp_port_;
  }

  std::move(pending_callback_).Run(fd >= 0);
}

void WorkspaceProfileApplier::OnHealthCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cdp_socket_ < 0) {
    // Already disconnected; stop health timer.
    cdp_health_timer_.Stop();
    return;
  }

  struct pollfd pfd;
  pfd.fd = cdp_socket_;
  pfd.events = POLLHUP | POLLERR | POLLRDHUP;
  int ret = poll(&pfd, 1, 0);

  if (ret < 0) {
    // poll() error — treat as disconnect.
    LOG(WARNING) << "CDP socket poll error on fd=" << cdp_socket_
                 << ", reconnecting";
    ReconnectCDP();
  } else if (ret > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLRDHUP))) {
    LOG(WARNING) << "CDP WebSocket fd=" << cdp_socket_
                 << " hung up, reconnecting";
    ReconnectCDP();
  }
}

void WorkspaceProfileApplier::ReconnectCDP() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cdp_health_timer_.Stop();
  if (cdp_socket_ >= 0) {
    close(cdp_socket_);
    cdp_socket_ = -1;
  }

  // Restore saved commands for re-send.
  pending_command_jsons_ = saved_command_jsons_;
  commands_started_ = false;
  poll_retries_left_ = 60;

  VLOG(1) << "Reconnecting CDP on port " << cdp_port_;
  cdp_poll_timer_.Start(
      FROM_HERE, base::Milliseconds(500),
      base::BindRepeating(&WorkspaceProfileApplier::OnPollTick,
                          base::Unretained(this)));
}

std::vector<base::DictValue> WorkspaceProfileApplier::GenerateCommandBatch(
    const std::vector<Workspace>& workspaces) const {
  std::vector<base::DictValue> all_commands;
  for (const auto& ws : workspaces) {
    auto commands = injector_->GenerateCDPCommands(ws);
    for (auto& cmd : commands) {
      all_commands.push_back(std::move(cmd));
    }
  }
  return all_commands;
}

}  // namespace purecloak
