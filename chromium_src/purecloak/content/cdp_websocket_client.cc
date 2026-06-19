// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/cdp_websocket_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace purecloak {

// Max HTTP response body size (256 KB — plenty for /json which is < 10 KB).
constexpr size_t kMaxHttpResponse = 262144;

// Max WebSocket frame payload (1 MB).
constexpr size_t kMaxWsPayload = 1048576;

// Socket read/write timeout in seconds.
constexpr int kSocketTimeoutSec = 5;

// ---------------------------------------------------------------------------
// Socket utilities
// ---------------------------------------------------------------------------

// static
int CDPWebSocketClient::TcpConnect(int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return -1;
  }

  // Set send/recv timeouts so blocking calls don't hang forever.
  struct timeval tv;
  tv.tv_sec = kSocketTimeoutSec;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
      0) {
    close(sock);
    return -1;
  }

  return sock;
}

// static
bool CDPWebSocketClient::SendAll(int sock, const void* buffer, size_t length) {
  const char* buf = static_cast<const char*>(buffer);
  size_t total = 0;
  while (total < length) {
    ssize_t n = UNSAFE_BUFFERS(send(sock, buf + total, length - total, 0));
    if (n <= 0) {
      return false;
    }
    total += static_cast<size_t>(n);
  }
  return true;
}

// static
bool CDPWebSocketClient::RecvAll(int sock, void* buffer, size_t length) {
  char* buf = static_cast<char*>(buffer);
  size_t total = 0;
  while (total < length) {
    ssize_t n = UNSAFE_BUFFERS(recv(sock, buf + total, length - total, 0));
    if (n <= 0) {
      return false;
    }
    total += static_cast<size_t>(n);
  }
  return true;
}

// ---------------------------------------------------------------------------
// HTTP transport
// ---------------------------------------------------------------------------

// static
std::string CDPWebSocketClient::HttpGet(int port, const std::string& path) {
  int sock = TcpConnect(port);
  if (sock < 0) {
    return "";
  }

  // Build HTTP/1.1 GET request.
  std::string request = base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: 127.0.0.1:%d\r\n"
      "Connection: close\r\n"
      "\r\n",
      path.c_str(), port);

  if (!SendAll(sock, request.data(), request.size())) {
    close(sock);
    return "";
  }

  // Read response until connection close or size limit.
  std::string response;
  char buf[4096];
  while (true) {
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }
    response.append(buf, static_cast<size_t>(n));
    if (response.size() > kMaxHttpResponse) {
      break;
    }
  }
  close(sock);

  // Extract body: skip HTTP status line + headers (terminated by \r\n\r\n).
  size_t header_end = response.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return "";
  }
  return response.substr(header_end + 4);
}

// ---------------------------------------------------------------------------
// WebSocket transport (RFC 6455)
// ---------------------------------------------------------------------------

// static
std::string CDPWebSocketClient::GenerateWebSocketKey() {
  std::string raw;
  raw.reserve(16);
  for (int i = 0; i < 16; ++i) {
    raw += static_cast<char>(base::RandIntInclusive(0, 255));
  }
  return base::Base64Encode(raw);
}

// static
bool CDPWebSocketClient::WebSocketHandshake(int sock,
                                              int port,
                                              const std::string& path) {
  std::string key = GenerateWebSocketKey();

  std::string request = base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: 127.0.0.1:%d\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n",
      path.c_str(), port, key.c_str());

  if (!SendAll(sock, request.data(), request.size())) {
    return false;
  }

  // Read handshake response until header terminator.
  std::string response;
  char buf[1024];
  while (response.find("\r\n\r\n") == std::string::npos) {
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) {
      return false;
    }
    response.append(buf, static_cast<size_t>(n));
    if (response.size() > 8192) {
      return false;  // Response too large.
    }
  }

  // Verify 101 Switching Protocols.
  return response.find("101") != std::string::npos &&
         response.find("Upgrade") != std::string::npos;
}

// static
bool CDPWebSocketClient::SendFrame(int sock, const std::string& payload) {
  // Generate random 4-byte masking key (RFC 6455 §5.3).
  uint8_t mask[4];
  mask[0] = static_cast<uint8_t>(base::RandIntInclusive(0, 255));
  mask[1] = static_cast<uint8_t>(base::RandIntInclusive(0, 255));
  mask[2] = static_cast<uint8_t>(base::RandIntInclusive(0, 255));
  mask[3] = static_cast<uint8_t>(base::RandIntInclusive(0, 255));

  std::string frame;
  frame.reserve(payload.size() + 14);

  // Byte 0: FIN=1, RSV=0, opcode=0x1 (text).
  frame += static_cast<char>(0x81);

  // Byte 1: MASK=1, payload length (7-bit, or 126/127 for extended).
  size_t len = payload.size();
  if (len <= 125) {
    frame += static_cast<char>(0x80 | static_cast<char>(len));
  } else if (len <= 65535) {
    frame += static_cast<char>(0x80 | 126);
    frame += static_cast<char>((len >> 8) & 0xFF);
    frame += static_cast<char>(len & 0xFF);
  } else {
    frame += static_cast<char>(0x80 | 127);
    // 64-bit length (high 4 bytes are zero for payloads < 4 GB).
    for (int i = 7; i >= 0; --i) {
      frame += static_cast<char>((len >> (i * 8)) & 0xFF);
    }
  }

  // Masking key (4 bytes).
  frame += static_cast<char>(mask[0]);
  frame += static_cast<char>(mask[1]);
  frame += static_cast<char>(mask[2]);
  frame += static_cast<char>(mask[3]);

  // Masked payload: XOR each byte with masking key (repeating).
  for (size_t i = 0; i < payload.size(); ++i) {
    frame += static_cast<char>(payload[i] ^ mask[i % 4]);
  }

  return SendAll(sock, frame.data(), frame.size());
}

// static
std::string CDPWebSocketClient::ReadFrame(int sock) {
  // Read frame header (first 2 bytes).
  uint8_t header[2];
  if (!RecvAll(sock, header, 2)) {
    return "";
  }

  uint8_t opcode = header[0] & 0x0F;

  // Handle close frame.
  if (opcode == 0x8) {
    return "";
  }
  // Ignore ping/pong frames (return empty to signal skip).
  if (opcode == 0x9 || opcode == 0xA) {
    // Still need to consume the frame to keep the stream in sync.
    // Fall through to read payload and discard.
  }

  bool masked = (header[1] & 0x80) != 0;
  uint64_t payload_len = header[1] & 0x7F;

  if (payload_len == 126) {
    uint8_t ext[2];
    if (!RecvAll(sock, ext, 2)) {
      return "";
    }
    payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
  } else if (payload_len == 127) {
    uint8_t ext[8];
    if (!RecvAll(sock, ext, 8)) {
      return "";
    }
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | UNSAFE_BUFFERS(ext[i]);
    }
  }

  if (payload_len > kMaxWsPayload) {
    LOG(ERROR) << "WebSocket frame payload too large: " << payload_len;
    return "";
  }

  uint8_t mask[4] = {0};
  if (masked) {
    if (!RecvAll(sock, mask, 4)) {
      return "";
    }
  }

  // Read payload.
  std::string payload;
  payload.resize(static_cast<size_t>(payload_len));
  if (payload_len > 0) {
    if (!RecvAll(sock, &payload[0], static_cast<size_t>(payload_len))) {
      return "";
    }
  }

  // Unmask if server masked (servers shouldn't mask per spec, but handle it).
  if (masked) {
    for (size_t i = 0; i < payload.size(); ++i) {
      payload[i] ^= mask[i % 4];
    }
  }

  // For ping/pong, return empty to signal the caller to try reading again.
  if (opcode == 0x9 || opcode == 0xA) {
    return "";
  }

  return payload;
}

// ---------------------------------------------------------------------------
// CDP discovery
// ---------------------------------------------------------------------------

// static
bool CDPWebSocketClient::IsCDPAvailable(int port) {
  std::string body = HttpGet(port, "/json/version");
  return !body.empty() && body.find("Browser") != std::string::npos;
}

// static
std::string CDPWebSocketClient::GetPageWebSocketUrl(int port) {
  std::string body = HttpGet(port, "/json");
  if (body.empty()) {
    return "";
  }

  auto parsed = base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_list()) {
    return "";
  }

  // Find the first page-type target.
  for (const auto& target : parsed->GetList()) {
    const std::string* type = target.GetDict().FindString("type");
    if (type && *type == "page") {
      const std::string* ws_url =
          target.GetDict().FindString("webSocketDebuggerUrl");
      if (ws_url && !ws_url->empty()) {
        return *ws_url;
      }
    }
  }

  // Fallback: return any target with a webSocketDebuggerUrl.
  for (const auto& target : parsed->GetList()) {
    const std::string* ws_url =
        target.GetDict().FindString("webSocketDebuggerUrl");
    if (ws_url && !ws_url->empty()) {
      return *ws_url;
    }
  }

  return "";
}

// ---------------------------------------------------------------------------
// Full CDP command pipeline
// ---------------------------------------------------------------------------

// static
int CDPWebSocketClient::SendCommands(int port,
                                     std::vector<std::string> command_jsons) {
  if (command_jsons.empty()) {
    return -1;
  }

  // Step 1: Discover page WebSocket URL.
  std::string ws_url = GetPageWebSocketUrl(port);
  if (ws_url.empty()) {
    LOG(ERROR) << "CDP: no page target found on port " << port;
    return -1;
  }

  // Parse ws://127.0.0.1:{port}{path} to extract path.
  std::string path = "/";
  size_t scheme_end = ws_url.find("://");
  if (scheme_end != std::string::npos) {
    std::string after_scheme = ws_url.substr(scheme_end + 3);
    size_t path_start = after_scheme.find('/');
    if (path_start != std::string::npos) {
      path = after_scheme.substr(path_start);
    }
  }

  // Step 2: TCP connect.
  int sock = TcpConnect(port);
  if (sock < 0) {
    LOG(ERROR) << "CDP: failed to connect to port " << port;
    return -1;
  }

  // Step 3: WebSocket handshake.
  if (!WebSocketHandshake(sock, port, path)) {
    LOG(ERROR) << "CDP: WebSocket handshake failed on port " << port;
    close(sock);
    return -1;
  }

  // Step 4: Parse command IDs from JSON, then send each command.
  // Parse all IDs first so we can match responses by ID later.
  std::vector<int> command_ids;
  command_ids.reserve(command_jsons.size());
  for (const auto& json : command_jsons) {
    auto parsed = base::JSONReader::Read(json, base::JSON_PARSE_RFC);
    if (parsed && parsed->is_dict()) {
      command_ids.push_back(parsed->GetDict().FindInt("id").value_or(-1));
    } else {
      command_ids.push_back(-1);
    }
  }

  bool all_success = true;
  for (size_t i = 0; i < command_jsons.size(); ++i) {
    if (!SendFrame(sock, command_jsons[i])) {
      LOG(ERROR) << "CDP: failed to send command " << i << " (id="
                 << command_ids[i] << ")";
      all_success = false;
      break;
    }

    // Read frames until we get a response with matching id.
    // Skip events (frames with "method" but no "id") and ping/pong.
    int expected_id = command_ids[i];
    bool got_response = false;
    for (int retry = 0; retry < 20; ++retry) {
      std::string response = ReadFrame(sock);
      if (response.empty()) {
        // Socket closed or ping/pong — try again.
        continue;
      }

      auto parsed = base::JSONReader::Read(response, base::JSON_PARSE_RFC);
      if (!parsed || !parsed->is_dict()) {
        // Malformed response — log and continue reading.
        LOG(WARNING) << "CDP: malformed response for command " << i;
        continue;
      }

      auto& dict = parsed->GetDict();
      int response_id = dict.FindInt("id").value_or(-1);

      if (response_id == expected_id) {
        // This is the response we're waiting for.
        got_response = true;
        if (dict.Find("error")) {
          const std::string* msg = dict.FindString("error.message");
          LOG(ERROR) << "CDP command " << i << " (id=" << expected_id
                     << ") ERROR: " << (msg ? *msg : "unknown");
        }
        break;
      }

      if (response_id == -1 && dict.FindString("method")) {
        // CDP event — skip silently.
        continue;
      }

      // Response with unexpected id — log and skip.
      LOG(WARNING) << "CDP: unexpected response id=" << response_id
                   << " (expected " << expected_id << "), skipping";
    }

    if (!got_response) {
      LOG(WARNING) << "CDP: no matching response for command " << i
                   << " (id=" << expected_id << ")";
    }
  }

  if (all_success) {
    VLOG(1) << "CDP: all " << command_jsons.size()
            << " commands sent successfully, keeping fd=" << sock
            << " open (Chrome 151+ session-scoped scripts)";
    return sock;
  }

  // On failure, close the socket and return -1.
  close(sock);
  return -1;
}

}  // namespace purecloak
