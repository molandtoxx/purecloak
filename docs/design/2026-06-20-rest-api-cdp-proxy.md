# PureCloak REST API + CDP Proxy — Design Spec

**Date**: 2026-06-20
**Status**: Draft
**Phase**: Phase 1 (P0)

---

## 1. Overview

Add a built-in REST API server and CDP WebSocket proxy to the PureCloak browser process, enabling external tools (CLI, Python SDK, Playwright, automation frameworks) to programmatically manage workspaces and connect to running browser instances.

### Goals
- External HTTP API for workspace CRUD and lifecycle management
- CDP WebSocket proxy for Playwright/Puppeteer `connectOverCDP` compatibility
- No external dependencies — all in C++ using Chromium's `net::HttpServer`
- Secure by default (localhost-only binding)

### Non-Goals
- Authentication/authorization (future phase)
- Rate limiting (future phase)
- HTTPS support (future phase)

---

## 2. Architecture

```
External Client (curl/Playwright/CLI)
        │
        │ HTTP/WS :9334
        ▼
┌─────────────────────────────────────┐
│         WorkspaceApiServer          │
│  (net::HttpServer on port 9334)    │
│                                     │
│  ┌───────────────────────────────┐  │
│  │  WorkspaceApiHandler          │  │
│  │  ├── GET  /api/workspaces     │  │
│  │  ├── POST /api/workspaces     │  │
│  │  ├── GET  /api/workspaces/{id}│  │
│  │  ├── PUT  /api/workspaces/{id}│  │
│  │  ├── DELETE /api/workspaces   │  │
│  │  │         /{id}              │  │
│  │  ├── POST /api/workspaces/{id}│  │
│  │  │         /launch            │  │
│  │  ├── POST /api/workspaces/{id}│  │
│  │  │         /stop              │  │
│  │  ├── GET  /api/workspaces/{id}│  │
│  │  │         /status            │  │
│  │  └── GET  /api/status         │  │
│  └──────────┬────────────────────┘  │
│             │                        │
│  ┌──────────▼────────────────────┐  │
│  │  CDP Proxy Handler            │  │
│  │  WS /api/workspaces/{id}/cdp  │  │
│  │     /json/version             │  │
│  │     /json/list                │  │
│  │     /devtools/...             │  │
│  └───────────────────────────────┘  │
└──────────────────┬──────────────────┘
                   │
                   ▼
      ┌────────────────────────┐
      │  WorkspaceStore        │
      │  RunningWorkspaceMgr   │
      │  (existing C++ code)   │
      └────────────────────────┘
```

### Data Flow

```
HTTP Request → net::ServerSocket → net::HttpServer
    → Delegate::OnHttpRequest()
    → WorkspaceApiHandler::Dispatch()
    → Route matching (path + method)
    → Call handler function
    → WorkspaceStore / RunningWorkspaceManager API
    → JSON response via Send200/Send404/Send500

WebSocket Upgrade → Delegate::OnWebSocketRequest()
    → CDPProxyHandler::HandleUpgrade()
    → Extract workspace_id from path
    → Lookup RunningWorkspace CDP port
    → AcceptWebSocket() on client side
    → Create outbound WS to target CDP port
    → Bidirectional relay via OnWebSocketMessage() + SendOverWebSocket()
```

---

## 3. API Specification

### 3.1 Port Configuration

```
Default port: 9334
CLI flag: --purecloak-api-port=N
```

Selected because it's outside the CDP port range (9333-9352) to avoid conflicts.

### 3.2 Common Response Format

```json
// Success
{ "success": true, "data": { ... } }

// Error
{ "success": false, "error": { "code": "ERROR_CODE", "message": "..." } }
```

### 3.3 Endpoints

#### `GET /api/workspaces`
List all workspaces with runtime status.

Response `200`:
```json
{
  "success": true,
  "data": {
    "workspaces": [
      {
        "id": "uuid-xxx",
        "name": "欧美店铺",
        "type": "fingerprint",
        "status": "running",
        "cdp_port": 9335,
        "cdp_url": "http://127.0.0.1:9335",
        "proxy": "socks5://...",
        "timezone": "America/New_York",
        "created_at": "...",
        "updated_at": "..."
      }
    ]
  }
}
```

#### `POST /api/workspaces`
Create a new workspace.

Request body:
```json
{
  "name": "欧美店铺",
  "type": "fingerprint",
  "proxy": "socks5://user:pass@host:1080",
  "timezone": "America/New_York",
  "locale": "en-US",
  "screen_width": 1920,
  "screen_height": 1080,
  "user_agent": "",
  "fingerprint_seed": 0,
  "headless": false,
  "auto_launch": false
}
```

Response `201`:
```json
{ "success": true, "data": { "id": "uuid-xxx", ... } }
```

#### `GET /api/workspaces/{id}`
Get single workspace details.

Response `200` / `404`.

#### `PUT /api/workspaces/{id}`
Update workspace fields. Only provided fields are updated (partial update).

Response `200` / `404`.

#### `DELETE /api/workspaces/{id}`
Delete workspace. If running, stop first.

Response `200` / `404`.

#### `POST /api/workspaces/{id}/launch`
Launch the workspace as a Chromium subprocess.

Response `200`:
```json
{
  "success": true,
  "data": {
    "status": "starting",
    "cdp_port": 9335,
    "cdp_url": "http://127.0.0.1:9335",
    "pid": 12345
  }
}
```

Error `409` if already running.

#### `POST /api/workspaces/{id}/stop`
Stop a running workspace.

Response `200` / `404`.

#### `GET /api/workspaces/{id}/status`
Get runtime status of a workspace.

Response `200`:
```json
{
  "success": true,
  "data": {
    "status": "running",
    "cdp_port": 9335,
    "uptime_seconds": 3600,
    "pid": 12345
  }
}
```

#### `GET /api/status`
System health check.

```json
{
  "success": true,
  "data": {
    "version": "1.0.0",
    "running_workspaces": 2,
    "total_workspaces": 5,
    "uptime_seconds": 86400
  }
}
```

Always accessible (no auth, for health checks).

---

## 4. CDP WebSocket Proxy

### 4.1 Primary Endpoint

```
WS /api/workspaces/{id}/cdp
```

Connects to the workspace's browser-level CDP WebSocket. Playwright usage:

```javascript
const browser = await chromium.connectOverCDP(
  'http://127.0.0.1:9334/api/workspaces/ws-xxx/cdp'
);
// Playwright GETs /json/version → we proxy with rewritten WS URLs
// Playwright WS connects → we relay to the workspace CDP
```

### 4.2 HTTP CDP Info Endpoints

These are proxied to the workspace's CDP HTTP server, with `webSocketDebuggerUrl` rewritten to point back through our proxy:

```
GET /api/workspaces/{id}/cdp/json/version  → proxy to workspace CDP /json/version
GET /api/workspaces/{id}/cdp/json/list      → proxy to workspace CDP /json/list
```

### 4.3 URL Rewriting

When proxying `/json/version` and `/json/list`, all `webSocketDebuggerUrl` values are rewritten:

```
Original: ws://127.0.0.1:9335/devtools/page/XXXX
Rewritten: ws://127.0.0.1:9334/api/workspaces/{id}/cdp/devtools/page/XXXX
```

This ensures Playwright's CDP auto-discovery works seamlessly.

### 4.4 WebSocket Relay

```
Client WS ──→ WorkspaceApiServer ──→ Target CDP WS (127.0.0.1:{cdp_port})
     ↑               │                       ↑
     │          OnWebSocketMessage       WebSocket send
     │               │                       │
     └───────── SendOverWebSocket ←─── WebSocket recv
```

The relay is fully transparent — all CDP messages pass through unmodified. The proxy maintains a map of `connection_id → {outbound_ws_fd}` for bidirectional routing.

---

## 5. File Layout

```
chromium_src/purecloak/browser/api/
├── BUILD.gn                           # GN build config
├── workspace_api_server.h             # Server lifecycle (Start/Stop)
├── workspace_api_server.cc
├── workspace_api_handler.h            # HTTP route dispatch
├── workspace_api_handler.cc
├── cdp_proxy_handler.h                # CDP WebSocket proxy
├── cdp_proxy_handler.cc
└── api_test.cc                        # Unit tests
```

### workspace_api_server.h
```cpp
class WorkspaceApiServer {
 public:
  WorkspaceApiServer(WorkspaceStore* store,
                     RunningWorkspaceManager* manager);
  ~WorkspaceApiServer();

  bool Start(int port);
  void Stop();

 private:
  std::unique_ptr<net::HttpServer> server_;
  std::unique_ptr<WorkspaceApiHandler> handler_;
  std::unique_ptr<base::Thread> thread_;
};
```

### workspace_api_handler.h
```cpp
class WorkspaceApiHandler : public net::HttpServer::Delegate {
  // net::HttpServer::Delegate:
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;
};
```

### cdp_proxy_handler.h
```cpp
class CDPProxyHandler {
  // Manages bidirectional WebSocket relay for CDP
  void HandleCDPProxyRequest(int connection_id,
                             const std::string& workspace_id,
                             const std::string& subpath);
  void HandleCDPHTTPRequest(int connection_id,
                            const std::string& workspace_id,
                            const std::string& subpath);
  
  // Bidirectional relay
  void OnCDPMessage(int cdp_fd, std::string data);
  void SendToClient(int connection_id, std::string data);
};
```

---

## 6. Build Integration

### BUILD.gn Changes

Add to `purecloak/browser/api/BUILD.gn`:
```python
source_set("api") {
  sources = [...]
  deps = [
    "//base",
    "//net",
    "//purecloak/browser",
    "//purecloak/browser/profiles",
  ]
}
```

Add to `purecloak/browser/BUILD.gn`:
```python
deps += [ "//purecloak/browser/api" ]
```

### Lifecycle Integration

Add to `PureCloakBrowserMainParts`:

```cpp
void PureCloakBrowserMainParts::PostCreateThreads() {
  // Start API server on a background thread
  api_server_ = std::make_unique<WorkspaceApiServer>(
      workspace_store_, workspace_manager_);
  api_server_->Start(port);
}

void PureCloakBrowserMainParts::PreMainMessageLoopRun() {
  // Ensure API server is ready before accepting requests
}

void PureCloakBrowserMainParts::PostDestroyThreads() {
  api_server_->Stop();
}
```

---

## 7. Error Handling

| HTTP Code | Condition |
|-----------|-----------|
| 200 | Success |
| 201 | Resource created |
| 400 | Invalid request body / missing required fields |
| 404 | Workspace not found |
| 409 | Already running (launch) / not running (stop) |
| 500 | Internal error (process launch failure, etc.) |

### Error Codes

| Code | Message |
|------|---------|
| `WORKSPACE_NOT_FOUND` | "Workspace {id} not found" |
| `ALREADY_RUNNING` | "Workspace {id} is already running" |
| `NOT_RUNNING` | "Workspace {id} is not running" |
| `MAX_CONCURRENT` | "Maximum concurrent workspaces reached" |
| `PORT_UNAVAILABLE` | "No available CDP port" |
| `LAUNCH_FAILED` | "Failed to launch workspace subprocess" |
| `INVALID_REQUEST` | "Invalid request body" |

---

## 8. Thread Safety

- `net::HttpServer` runs on its own `base::Thread("PureCloakAPI")` 
- All accesses to `WorkspaceStore` and `RunningWorkspaceManager` are posted to the UI thread via `PostTask`
- Launch/Stop operations are inherently async (they wait for subprocess)
- HTTP responses for async operations use a callback pattern:
  1. Client sends POST /launch
  2. Handler posts task to UI thread
  3. UI thread calls RunningWorkspaceManager::Start()
  4. Start callback posts result back to API thread
  5. API thread sends HTTP response

---

## 9. Testing

### Unit Tests (`api_test.cc`)
- Route matching (path parsing)
- Request validation (missing fields)
- Error response format
- CDP URL rewriting

### Integration Tests
- Launch workspace via API → verify CDP port is allocated
- Launch → stop → verify cleanup
- Create workspace → verify it appears in list
- WebSocket CDP proxy → connect and verify message relay

### Manual Testing
```bash
# Create workspace
curl -X POST http://127.0.0.1:9334/api/workspaces \
  -H 'Content-Type: application/json' \
  -d '{"name":"test","type":"fingerprint"}'

# Launch
curl -X POST http://127.0.0.1:9334/api/workspaces/{id}/launch

# CDP connect via proxy
/path/to/chromium http://127.0.0.1:9334/api/workspaces/{id}/cdp/json/version
```

---

## 10. Future Considerations

- **Authentication**: Token-based auth via `X-API-Key` header (when PureCloak supports multi-user)
- **HTTPS**: Optional TLS termination for remote access
- **Rate limiting**: Per-connection rate limiting to prevent abuse
- **Event streaming**: SSE endpoint for workspace status change events (alternative to polling)
