// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "purecloak/content/cdp_websocket_client.h"

namespace purecloak {

namespace {

int FindFreePort() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return 0;

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(sock);
    return 0;
  }

  socklen_t len = sizeof(addr);
  getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &len);
  int port = ntohs(addr.sin_port);
  close(sock);
  return port;
}

base::FilePath GetChromeBinaryPath() {
  const char* env = std::getenv("PURECLOAK_BIN");
  if (env && env[0])
    return base::FilePath(env);
  const char* env2 = std::getenv("CHROME_BIN");
  if (env2 && env2[0])
    return base::FilePath(env2);
  return base::FilePath("out/purecloak/purecloak");
}

}  // namespace

class CDPIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chrome_path_ = GetChromeBinaryPath();
    ASSERT_TRUE(base::PathExists(chrome_path_))
        << "PureCloak binary not found at: " << chrome_path_.value()
        << "\nSet PURECLOAK_BIN env var to override.";

    cdp_port_ = FindFreePort();
    ASSERT_GT(cdp_port_, 0) << "Failed to find a free port.";

    ASSERT_TRUE(base::CreateNewTempDirectory(
        "purecloak_integration_test_", &user_data_dir_))
        << "Failed to create temp user-data-dir.";
  }

  void TearDown() override {
    if (process_.IsValid()) {
      process_.Terminate(0, true);
    }
    if (!user_data_dir_.empty()) {
      base::DeletePathRecursively(user_data_dir_);
    }
  }

  bool WaitForCDP(base::TimeDelta timeout) {
    base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
    while (base::TimeTicks::Now() < deadline) {
      if (CDPWebSocketClient::IsCDPAvailable(cdp_port_)) {
        return true;
      }
      base::PlatformThread::Sleep(base::Milliseconds(500));
    }
    return false;
  }

  base::FilePath chrome_path_;
  base::FilePath user_data_dir_;
  int cdp_port_ = 0;
  base::Process process_;
};

TEST_F(CDPIntegrationTest, ChromeStartsAndCDPBecomesAvailable) {
  base::CommandLine cmd(chrome_path_);
  cmd.AppendSwitchASCII("headless", "new");
  cmd.AppendSwitch("no-first-run");
  cmd.AppendSwitch("no-default-browser-check");
  cmd.AppendSwitch("disable-gpu");
  cmd.AppendSwitch("no-sandbox");
  cmd.AppendSwitchASCII("remote-debugging-port",
                        base::NumberToString(cdp_port_));
  cmd.AppendSwitchASCII("user-data-dir", user_data_dir_.value());

  base::LaunchOptions options;
  process_ = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(process_.IsValid())
      << "Failed to launch Chrome process.";

  EXPECT_TRUE(WaitForCDP(base::Seconds(30)))
      << "CDP endpoint did not become available within 30s on port "
      << cdp_port_;
}

TEST_F(CDPIntegrationTest, GetPageWebSocketUrlReturnsNonEmpty) {
  base::CommandLine cmd(chrome_path_);
  cmd.AppendSwitchASCII("headless", "new");
  cmd.AppendSwitch("no-first-run");
  cmd.AppendSwitch("no-default-browser-check");
  cmd.AppendSwitch("disable-gpu");
  cmd.AppendSwitch("no-sandbox");
  cmd.AppendSwitchASCII("remote-debugging-port",
                        base::NumberToString(cdp_port_));
  cmd.AppendSwitchASCII("user-data-dir", user_data_dir_.value());

  base::LaunchOptions options;
  process_ = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(process_.IsValid());

  ASSERT_TRUE(WaitForCDP(base::Seconds(30)));

  std::string ws_url = CDPWebSocketClient::GetPageWebSocketUrl(cdp_port_);
  EXPECT_FALSE(ws_url.empty())
      << "Expected non-empty webSocketDebuggerUrl from /json endpoint";
  EXPECT_NE(std::string::npos, ws_url.find("ws://"))
      << "WebSocket URL should start with ws://";
}

TEST_F(CDPIntegrationTest, SendTargetGetTargetsCommand) {
  base::CommandLine cmd(chrome_path_);
  cmd.AppendSwitchASCII("headless", "new");
  cmd.AppendSwitch("no-first-run");
  cmd.AppendSwitch("no-default-browser-check");
  cmd.AppendSwitch("disable-gpu");
  cmd.AppendSwitch("no-sandbox");
  cmd.AppendSwitchASCII("remote-debugging-port",
                        base::NumberToString(cdp_port_));
  cmd.AppendSwitchASCII("user-data-dir", user_data_dir_.value());

  base::LaunchOptions options;
  process_ = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(process_.IsValid());

  ASSERT_TRUE(WaitForCDP(base::Seconds(30)));

  std::string command =
      R"({"id":1,"method":"Target.getTargets","params":{}})";
  bool success = CDPWebSocketClient::SendCommands(cdp_port_, {command});
  EXPECT_TRUE(success) << "Failed to send CDP command Target.getTargets";
}

TEST_F(CDPIntegrationTest, SendMultipleCDPCommands) {
  base::CommandLine cmd(chrome_path_);
  cmd.AppendSwitchASCII("headless", "new");
  cmd.AppendSwitch("no-first-run");
  cmd.AppendSwitch("no-default-browser-check");
  cmd.AppendSwitch("disable-gpu");
  cmd.AppendSwitch("no-sandbox");
  cmd.AppendSwitchASCII("remote-debugging-port",
                        base::NumberToString(cdp_port_));
  cmd.AppendSwitchASCII("user-data-dir", user_data_dir_.value());

  base::LaunchOptions options;
  process_ = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(process_.IsValid());

  ASSERT_TRUE(WaitForCDP(base::Seconds(30)));

  std::vector<std::string> commands = {
      R"({"id":1,"method":"Target.getTargets","params":{}})",
      R"({"id":2,"method":"Page.enable","params":{}})",
      R"({"id":3,"method":"Runtime.enable","params":{}})",
  };

  bool success = CDPWebSocketClient::SendCommands(cdp_port_, commands);
  EXPECT_TRUE(success) << "Failed to send multiple CDP commands";
}

TEST_F(CDPIntegrationTest, CDPNotAvailableOnUnusedPort) {
  int unused_port = FindFreePort();
  ASSERT_GT(unused_port, 0);

  EXPECT_FALSE(CDPWebSocketClient::IsCDPAvailable(unused_port))
      << "CDP should not be available on an unused port";
}

}  // namespace purecloak
