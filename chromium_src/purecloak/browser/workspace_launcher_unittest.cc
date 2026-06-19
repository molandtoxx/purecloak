// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/workspace_launcher.h"

#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

// ─── RunningWorkspace::StatusToString ───────────────────────────────────────

TEST(RunningWorkspaceStatusTest, StatusToStringStarting) {
  EXPECT_EQ("starting",
            RunningWorkspace::StatusToString(
                RunningWorkspace::Status::kStarting));
}

TEST(RunningWorkspaceStatusTest, StatusToStringRunning) {
  EXPECT_EQ("running",
            RunningWorkspace::StatusToString(
                RunningWorkspace::Status::kRunning));
}

TEST(RunningWorkspaceStatusTest, StatusToStringStopped) {
  EXPECT_EQ("stopped",
            RunningWorkspace::StatusToString(
                RunningWorkspace::Status::kStopped));
}

TEST(RunningWorkspaceStatusTest, StatusToStringCrashed) {
  EXPECT_EQ("crashed",
            RunningWorkspace::StatusToString(
                RunningWorkspace::Status::kCrashed));
}

// ─── RunningWorkspace::StringToStatus ───────────────────────────────────────

TEST(RunningWorkspaceStatusTest, StringToStatusStarting) {
  EXPECT_EQ(RunningWorkspace::Status::kStarting,
            RunningWorkspace::StringToStatus("starting"));
}

TEST(RunningWorkspaceStatusTest, StringToStatusRunning) {
  EXPECT_EQ(RunningWorkspace::Status::kRunning,
            RunningWorkspace::StringToStatus("running"));
}

TEST(RunningWorkspaceStatusTest, StringToStatusStopped) {
  EXPECT_EQ(RunningWorkspace::Status::kStopped,
            RunningWorkspace::StringToStatus("stopped"));
}

TEST(RunningWorkspaceStatusTest, StringToStatusCrashed) {
  EXPECT_EQ(RunningWorkspace::Status::kCrashed,
            RunningWorkspace::StringToStatus("crashed"));
}

TEST(RunningWorkspaceStatusTest, StringToStatusInvalidDefaultsToStopped) {
  EXPECT_EQ(RunningWorkspace::Status::kStopped,
            RunningWorkspace::StringToStatus("invalid"));
  EXPECT_EQ(RunningWorkspace::Status::kStopped,
            RunningWorkspace::StringToStatus(""));
}

// ─── StatusToString / StringToStatus round-trip ─────────────────────────────

TEST(RunningWorkspaceStatusTest, StatusRoundTrip) {
  for (auto status : {RunningWorkspace::Status::kStarting,
                       RunningWorkspace::Status::kRunning,
                       RunningWorkspace::Status::kStopped,
                       RunningWorkspace::Status::kCrashed}) {
    std::string str = RunningWorkspace::StatusToString(status);
    EXPECT_EQ(status, RunningWorkspace::StringToStatus(str))
        << "Round-trip failed for status: " << str;
  }
}

// ─── RunningWorkspace::ToDict ───────────────────────────────────────────────

TEST(RunningWorkspaceTest, ToDictContainsId) {
  RunningWorkspace ws;
  ws.workspace_id = "ws-123";
  base::DictValue dict = ws.ToDict();

  EXPECT_EQ("ws-123", *dict.FindString("workspace_id"));
}

TEST(RunningWorkspaceTest, ToDictContainsStatus) {
  RunningWorkspace ws;
  ws.status = RunningWorkspace::Status::kRunning;
  base::DictValue dict = ws.ToDict();

  EXPECT_EQ("running", *dict.FindString("status"));
}

TEST(RunningWorkspaceTest, ToDictContainsCdpPort) {
  RunningWorkspace ws;
  ws.cdp_port = 9333;
  base::DictValue dict = ws.ToDict();

  // cdp_port may be stored as int or string; check for presence.
  EXPECT_TRUE(dict.FindInt("cdp_port") || dict.FindString("cdp_port"));
  if (auto port = dict.FindInt("cdp_port")) {
    EXPECT_EQ(9333, *port);
  }
}

TEST(RunningWorkspaceTest, ToDictContainsCdpUrl) {
  RunningWorkspace ws;
  ws.cdp_url = "http://127.0.0.1:9333";
  base::DictValue dict = ws.ToDict();

  EXPECT_EQ("http://127.0.0.1:9333", *dict.FindString("cdp_url"));
}

TEST(RunningWorkspaceTest, ToDictContainsUptimeSeconds) {
  RunningWorkspace ws;
  ws.launched_at = base::Time::Now();
  base::DictValue dict = ws.ToDict();

  EXPECT_TRUE(dict.FindInt("uptime_seconds"));
}

// ─── RunningWorkspace::Uptime ───────────────────────────────────────────────

TEST(RunningWorkspaceTest, UptimeZeroJustLaunched) {
  RunningWorkspace ws;
  ws.launched_at = base::Time::Now();
  base::TimeDelta uptime = ws.Uptime();

  // Just launched — uptime should be very small (under 1 second).
  EXPECT_LT(uptime.InSeconds(), 1);
}

TEST(RunningWorkspaceTest, UptimeNonZeroAfterDelay) {
  RunningWorkspace ws;
  // Set launched_at 60 seconds in the past.
  ws.launched_at = base::Time::Now() - base::Seconds(60);
  base::TimeDelta uptime = ws.Uptime();

  EXPECT_GE(uptime.InSeconds(), 59);
  EXPECT_LE(uptime.InSeconds(), 61);
}

TEST(RunningWorkspaceTest, UptimeNeverLaunched) {
  RunningWorkspace ws;
  // launched_at is default-constructed (epoch).
  base::TimeDelta uptime = ws.Uptime();

  // Should be a very large value (time since epoch) or zero.
  // Either way, it shouldn't crash.
  EXPECT_GE(uptime.InSeconds(), 0);
}

// ─── RunningWorkspace defaults ──────────────────────────────────────────────

TEST(RunningWorkspaceTest, DefaultStatusIsStarting) {
  RunningWorkspace ws;
  EXPECT_EQ(RunningWorkspace::Status::kStarting, ws.status);
}

TEST(RunningWorkspaceTest, DefaultCdpPortIsZero) {
  RunningWorkspace ws;
  EXPECT_EQ(0, ws.cdp_port);
}

TEST(RunningWorkspaceTest, DefaultCdpUrlIsEmpty) {
  RunningWorkspace ws;
  EXPECT_TRUE(ws.cdp_url.empty());
}

}  // namespace purecloak
