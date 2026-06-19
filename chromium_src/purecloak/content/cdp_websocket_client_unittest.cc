// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/cdp_websocket_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <thread>

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Minimal mock CDP HTTP server that responds to /json/version and /json.
// Runs in a background thread, accepts one connection per request.
class MockCDPServer {
 public:
  MockCDPServer() = default;
  ~MockCDPServer() { Stop(); }

  bool Start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
      return false;

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0) {
      close(listen_fd_);
      listen_fd_ = -1;
      return false;
    }

    socklen_t len = sizeof(addr);
    getsockname(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
    port_ = ntohs(addr.sin_port);

    listen(listen_fd_, 5);

    running_ = true;
    server_thread_ = std::thread([this] { ServerLoop(); });
    return true;
  }

  void Stop() {
    if (!running_)
      return;
    running_ = false;
    int wakeup_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (wakeup_fd >= 0) {
      struct sockaddr_in addr = {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = htons(static_cast<uint16_t>(port_));
      connect(wakeup_fd, reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr));
      close(wakeup_fd);
    }
    if (listen_fd_ >= 0) {
      close(listen_fd_);
      listen_fd_ = -1;
    }
    if (server_thread_.joinable())
      server_thread_.join();
  }

  int port() const { return port_; }

  void set_version_response(const std::string& body) {
    version_body_ = body;
  }

  void set_targets_response(const std::string& body) { targets_body_ = body; }

 private:
  void ServerLoop() {
    while (running_) {
      int client_fd = accept(listen_fd_, nullptr, nullptr);
      if (client_fd < 0)
        break;

      struct timeval tv;
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      char buf[4096] = {};
      ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
      if (n <= 0) {
        close(client_fd);
        continue;
      }

      std::string request(buf, static_cast<size_t>(n));

      std::string response_body;
      if (request.find("GET /json/version") != std::string::npos) {
        response_body = version_body_;
      } else if (request.find("GET /json") != std::string::npos) {
        response_body = targets_body_;
      } else {
        close(client_fd);
        continue;
      }

      std::string response = base::StringPrintf(
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: application/json\r\n"
          "Content-Length: %zu\r\n"
          "Connection: close\r\n"
          "\r\n"
          "%s",
          response_body.size(), response_body.c_str());

      send(client_fd, response.data(), response.size(), 0);
      close(client_fd);
    }
  }

  int listen_fd_ = -1;
  int port_ = 0;
  bool running_ = false;
  std::thread server_thread_;
  std::string version_body_ =
      R"({"Browser":"Chrome/130.0","webSocketDebuggerUrl":"ws://127.0.0.1:0/devtools/browser/fake"})";
  std::string targets_body_ =
      R"([{"type":"page","url":"about:blank","webSocketDebuggerUrl":"ws://127.0.0.1:0/devtools/page/fake-id","id":"fake-id"}])";
};

}  // namespace

// ─── IsCDPAvailable ─────────────────────────────────────────────────────────

TEST(CDPWebSocketClientTest, IsCDPAvailableFalseForClosedPort) {
  int port = FindFreePort();
  ASSERT_GT(port, 0);
  EXPECT_FALSE(CDPWebSocketClient::IsCDPAvailable(port));
}

TEST(CDPWebSocketClientTest, IsCDPAvailableFalseForZeroPort) {
  EXPECT_FALSE(CDPWebSocketClient::IsCDPAvailable(0));
}

TEST(CDPWebSocketClientTest, IsCDPAvailableTrueForMockServer) {
  MockCDPServer server;
  ASSERT_TRUE(server.Start());

  EXPECT_TRUE(CDPWebSocketClient::IsCDPAvailable(server.port()));

  server.Stop();
}

TEST(CDPWebSocketClientTest, IsCDPAvailableFalseForBadResponse) {
  MockCDPServer server;
  server.set_version_response(R"({"error":"not found"})");
  ASSERT_TRUE(server.Start());

  EXPECT_FALSE(CDPWebSocketClient::IsCDPAvailable(server.port()))
      << "Response without 'Browser' key should fail";

  server.Stop();
}

// ─── GetPageWebSocketUrl ────────────────────────────────────────────────────

TEST(CDPWebSocketClientTest, GetPageWebSocketUrlFromMockServer) {
  MockCDPServer server;
  ASSERT_TRUE(server.Start());

  std::string url = CDPWebSocketClient::GetPageWebSocketUrl(server.port());
  EXPECT_FALSE(url.empty());
  EXPECT_NE(std::string::npos, url.find("ws://"));
  EXPECT_NE(std::string::npos, url.find("page"));

  server.Stop();
}

TEST(CDPWebSocketClientTest, GetPageWebSocketUrlEmptyForClosedPort) {
  int port = FindFreePort();
  ASSERT_GT(port, 0);
  std::string url = CDPWebSocketClient::GetPageWebSocketUrl(port);
  EXPECT_TRUE(url.empty());
}

TEST(CDPWebSocketClientTest, GetPageWebSocketUrlNoPageTargets) {
  MockCDPServer server;
  server.set_targets_response(
      R"([{"type":"browser","webSocketDebuggerUrl":"ws://127.0.0.1:0/devtools/browser/x"}])");
  ASSERT_TRUE(server.Start());

  std::string url = CDPWebSocketClient::GetPageWebSocketUrl(server.port());
  EXPECT_FALSE(url.empty())
      << "Should fall back to any target with webSocketDebuggerUrl";

  server.Stop();
}

TEST(CDPWebSocketClientTest, GetPageWebSocketUrlEmptyTargetsList) {
  MockCDPServer server;
  server.set_targets_response("[]");
  ASSERT_TRUE(server.Start());

  std::string url = CDPWebSocketClient::GetPageWebSocketUrl(server.port());
  EXPECT_TRUE(url.empty()) << "Empty targets list should return empty URL";

  server.Stop();
}

TEST(CDPWebSocketClientTest, GetPageWebSocketUrlMalformedJson) {
  MockCDPServer server;
  server.set_targets_response("not valid json {{{");
  ASSERT_TRUE(server.Start());

  std::string url = CDPWebSocketClient::GetPageWebSocketUrl(server.port());
  EXPECT_TRUE(url.empty()) << "Malformed JSON should return empty URL";

  server.Stop();
}

// ─── SendCommands ───────────────────────────────────────────────────────────

TEST(CDPWebSocketClientTest, SendCommandsEmptyReturnsNegative) {
  EXPECT_LT(CDPWebSocketClient::SendCommands(99999, {}), 0);
}

TEST(CDPWebSocketClientTest, SendCommandsFailsForNoServer) {
  int port = FindFreePort();
  ASSERT_GT(port, 0);
  std::string cmd = R"({"id":1,"method":"Target.getTargets","params":{}})";
  EXPECT_LT(CDPWebSocketClient::SendCommands(port, {cmd}), 0);
}

}  // namespace purecloak
