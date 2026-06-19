// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_CONTENT_CDP_WEBSOCKET_CLIENT_H_
#define PURECLOAK_CONTENT_CDP_WEBSOCKET_CLIENT_H_

#include <string>
#include <vector>

namespace purecloak {

// Low-level CDP (Chrome DevTools Protocol) WebSocket client.
//
// Implements minimal HTTP/1.1 + RFC 6455 WebSocket transport for sending
// CDP commands to a Chrome subprocess's DevTools endpoint on localhost.
//
// ALL methods are BLOCKING and must be called from a background sequence
// (base::ThreadPool with base::MayBlock()).
//
// Flow:
//   1. IsCDPAvailable(port)  → GET /json/version (health check)
//   2. GetPageWebSocketUrl(port) → GET /json (discover page target)
//   3. SendCommands(port, jsons) → full pipeline: discover → WS handshake → send
class CDPWebSocketClient {
 public:
  CDPWebSocketClient() = delete;

  // Checks if the CDP HTTP endpoint is responding.
  // Returns true if GET /json/version returns valid JSON with a "Browser" key.
  static bool IsCDPAvailable(int port);

  // Discovers the webSocketDebuggerUrl for the first page-type target.
  // Returns empty string if no page target exists or on connection failure.
  static std::string GetPageWebSocketUrl(int port);

  // Full command pipeline: discover target → WebSocket connect → send commands.
  // |command_jsons| are pre-serialized JSON-RPC messages (with "id" included).
  // On success returns the WebSocket socket fd (>= 0). The caller MUST keep
  // this fd open — Chrome 151+ removes session-scoped scripts like
  // Page.addScriptToEvaluateOnNewDocument when the WebSocket connection
  // closes. Returns -1 on failure (caller should NOT close the fd).
  static int SendCommands(int port,
                          std::vector<std::string> command_jsons);

 private:
  // --- HTTP transport ---

  // Sends GET request to 127.0.0.1:{port}{path} and returns the response body.
  // Returns empty string on connection or read failure.
  static std::string HttpGet(int port, const std::string& path);

  // --- Socket utilities ---

  // Opens a TCP socket to 127.0.0.1:{port} with 5s read/write timeouts.
  // Returns socket fd, or -1 on failure.
  static int TcpConnect(int port);

  // Sends all bytes reliably (loops through partial sends).
  static bool SendAll(int sock, const void* buffer, size_t length);

  // Receives exactly |length| bytes (loops through partial reads).
  static bool RecvAll(int sock, void* buffer, size_t length);

  // --- WebSocket transport (RFC 6455) ---

  // Performs WebSocket client handshake over an open TCP socket.
  // |path| is the WebSocket URL path (e.g. "/devtools/page/{id}").
  // Returns true on receiving HTTP 101 Switching Protocols.
  static bool WebSocketHandshake(int sock, int port, const std::string& path);

  // Sends a WebSocket text frame with client-side masking (RFC 6455 §5.3).
  static bool SendFrame(int sock, const std::string& payload);

  // Reads one WebSocket text frame from the server.
  // Returns the payload string, or empty string on close/error.
  static std::string ReadFrame(int sock);

  // Generates a random 16-byte base64-encoded Sec-WebSocket-Key.
  static std::string GenerateWebSocketKey();
};

}  // namespace purecloak

#endif  // PURECLOAK_CONTENT_CDP_WEBSOCKET_CLIENT_H_
