# PureCloak C++ Anti-Detection Patches — Design Spec

**Date**: 2026-06-20
**Status**: Draft
**Phase**: Phase 1 (P0)

---

## 1. Motivation

PureCloak currently injects anti-detection scripts via CDP `Page.addScriptToEvaluateOnNewDocument`. This JS-level approach is detectable via:
- `Function.prototype.toString.call()` on replaced functions
- `Object.getOwnPropertyDescriptor()` revealing proxy getters
- Stack trace analysis exposing injected wrappers
- Timing analysis between native and wrapped calls

CloakBrowser solves this via **58 C++ patches** to the Chromium source (Blink/V8/GPU/Network layers). PureCloak, as a Chromium fork, must port ALL 58 patches to achieve equivalent stealth.

### What's Already Implemented in PureCloak

| Feature | Current Approach | Status |
|---------|-----------------|--------|
| `--force-webrtc-ip-handling-policy` | C++ flag in BuildCommandLine() | ✅ Done |
| Canvas/WebGL/Audio/Font protection | JS via AntiDetectionEngine | ⚠️ JS-level (detectable) |
| Screen/Timezone/Locale | CDP Emulation API | ⚠️ CDP-level (detectable) |
| Navigator overrides (UA, platform) | CDP Page.addScriptToEvaluateOnNewDocument | ⚠️ JS-level (detectable) |
| Proxy | `--proxy-server` flag | ✅ Done |

---

## 2. Complete Patch Inventory (58 patches)

### 2.1 Automation Signal Removal (5 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 1 | `navigator.webdriver` → false | `third_party/blink/renderer/modules/webdriver/navigator_webdriver.cc` | Always returns `false` |
| 2 | `--enable-automation` suppression | `content/browser/browser_main_loop.cc` | Flag has no effect; no automation markers exposed |
| 3 | CDP detection removal | `content/browser/devtools/devtools_agent_host.cc` | `isAutomatedWithCDP: false` |
| 4 | CDP input stealth (mouse/keyboard/touch/wheel) | `content/browser/renderer_host/input/*.cc`, `ui/events/blink/*.cc` | Removes automation signals from all input events |
| 5 | General automation marker reduction | Multiple `content/` and `chrome/` files | Reduces all automation-visible signals |

**Build flag**: `BUILDFLAG(IS_PURECLOAK)` guard in each file.

### 2.2 Canvas Fingerprinting (5 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 6 | Canvas 2D noise injection | `third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.cc` | ±1 RGB noise on ~0.1% pixels, seed-driven |
| 7 | `toDataURL()` spoofing | Same as above | Output hash varies per seed deterministically |
| 8 | `toBlob()` spoofing | Same as above | Blob output varies per seed |
| 9 | `getImageData()` noise | `third_party/blink/renderer/modules/canvas/canvas2d/image_data.cc` | Pixel data noise in same pattern |
| 10 | Canvas emoji rendering fix | `third_party/blink/renderer/platform/fonts/*.cc` | Font-dependent canvas hashes match real browsers |

**Seed mechanism**: `PureCloakSeed::Get()->seed()` drives deterministic PRNG.

### 2.3 WebGL Fingerprinting (8 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 11 | `UNMASKED_VENDOR_WEBGL` spoofing | `gpu/command_buffer/service/gles2_cmd_decoder.cc`, `third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc` | Returns seeded vendor string |
| 12 | `UNMASKED_RENDERER_WEBGL` spoofing | Same as above | Returns seeded renderer string |
| 13 | Renderer suffix fix | `gpu/config/gpu_info_collector.cc` | Removes driver version from renderer string |
| 14 | WebGL accuracy improvements | `gpu/command_buffer/service/*.cc` | Rendering accuracy matches real Chrome |
| 15 | WebGL format consistency | `third_party/blink/renderer/modules/webgl/webgl_rendering_context.cc` | Format consistency across APIs |
| 16 | GPU capability accuracy (NVIDIA) | `gpu/config/gpu_driver_bug_list.json`, `gpu_config/gpu_info_collector.cc` | Capability values match real hardware |
| 17 | GPU model database expansion | `gpu/config/software_rendering_list.json` | Expanded GPU models for diversity |
| 18 | macOS GPU accuracy | `gpu/config/gpu_info_collector_mac.mm` | Apple Silicon GPU profiles |

### 2.4 Audio Fingerprinting (4 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 19 | AudioContext noise injection | `third_party/blink/renderer/modules/webaudio/audio_context.cc` | ±0.0001 noise on ~1% of samples |
| 20 | OfflineAudioContext consistency | `third_party/blink/renderer/modules/webaudio/offline_audio_context.cc` | Deterministic seed-driven output |
| 21 | Offline audio rendering fix | Same as above | Consistency fixes |
| 22 | AAC audio encoder spoofing | `media/filters/aac_audio_encoder.cc`, `media/base/audio_encoder.cc` | Encoder fingerprint spoofing |

### 2.5 Font Fingerprinting (4 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 23 | Font enumeration spoofing | `third_party/blink/renderer/platform/fonts/font_cache.cc` | `--fingerprint-fonts-dir` controls visible fonts |
| 24 | Cross-platform font hiding | Same area | Hides platform-specific fonts when spoofing other platform |
| 25 | Font rendering accuracy (Windows) | `third_party/blink/renderer/platform/fonts/font_rendering.cc` | Matches real Windows Chrome |
| 26 | Cross-platform font edge case | Same as above | Edge case fixes |

### 2.6 GPU / WebGPU Fingerprinting (7 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 27 | WebGPU adapter spoofing | `third_party/dawn/src/dawn/native/*.cc` | Spoofs GPU adapter for headless/Docker |
| 28 | WebGPU adapter features | `third_party/dawn/src/dawn/native/Adapter.cc` | Spoofs features, limits, device ID |
| 29 | WebGPU adapter limits | Same as above | Fixed limits for NVIDIA profiles |
| 30 | WebGPU blocklist bypass | `gpu/config/gpu_blocklist.xml` | Auto-bypass (safe with full spoofing) |
| 31 | GPU renderer suffix strings | `gpu/config/gpu_info_collector.cc` | Match real Chrome output across platforms |
| 32 | Windows native GPU passthrough | `gpu/config/gpu_info_collector_win.cc` | Real values pass through directly |
| 33 | GPU/display/graphics params | `gpu/config/*.cc`, `chrome/browser/gpu/*.cc` | Corrected to match stock Chrome 146 |

### 2.7 Screen / Display Fingerprinting (6 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 34 | Screen width spoofing | `third_party/blink/renderer/core/frame/screen.cc` | `screen.width` → seeded value |
| 35 | Screen height spoofing | Same as above | `screen.height` → seeded value |
| 36 | Taskbar height spoofing | Same as above | `screen.availHeight` adjusts for taskbar |
| 37 | outerHeight calculation fix | `third_party/blink/renderer/core/frame/local_dom_window.cc` | Fixed for non-incognito |
| 38 | Platform-aware screen defaults | Same as Screen `screen.cc` | Dimensions auto-adjust per platform |
| 39 | Window position spoofing | `chrome/browser/ui/browser_window.cc` | Window coordinates spoofing |

### 2.8 Hardware Reporting (4 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 40 | `navigator.hardwareConcurrency` spoofing | `third_party/blink/renderer/core/frame/navigator.cc` | Seeded value (default 8) |
| 41 | `navigator.deviceMemory` spoofing | `third_party/blink/renderer/core/frame/navigator.cc` | Seeded value in GB (default 8) |
| 42 | Device memory flag | `content/browser/browser_main.cc` | `--fingerprint-device-memory` CLI flag |
| 43 | Platform-aware hardware defaults | `third_party/blink/renderer/core/frame/navigator.cc` | HW specs auto-match platform |

### 2.9 User-Agent / Brand Spoofing (5 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 44 | UA string spoofing | `content/browser/browser_main.cc`, `net/url_request/url_request.cc` | No `HeadlessChrome` leak |
| 45 | Browser brand spoofing | `chrome/browser/chrome_content_browser_client.cc` | Chrome/Edge/Opera/Vivaldi |
| 46 | Brand version spoofing | `components/embedder_support/user_agent_utils.cc` | UA + Client Hints version |
| 47 | Platform version spoofing | Same as above | Client Hints platform version |
| 48 | Brand string format fix | `chrome/browser/chrome_content_browser_client.cc` | Matches Chrome format |

### 2.10 WebRTC Fingerprinting (3 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 49 | WebRTC IP spoofing | `third_party/webrtc/pc/peer_connection.cc`, `p2p/base/port.cc` | ICE candidates replace real IP |
| 50 | ICE candidate replacement | `p2p/base/basic_port_allocator.cc` | Spoofed IP instead of local IP |
| 51 | Native SOCKS5 with UDP ASSOCIATE | `net/socket/socks5_client_socket.cc`, `net/quic/*.cc` | QUIC/HTTP3 through SOCKS5 |

### 2.11 Network / Proxy Signals (3 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 52 | Network timing normalization | `net/http/http_network_transaction.cc`, `net/dns/host_resolver.cc` | DNS/connect/SSL timing zeroed with proxy |
| 53 | Proxy cache header stripping | `net/http/http_request_headers.cc` | Cache headers stripped from requests |
| 54 | Proxy-Connection header leak removal | Same as above | Header removed |

### 2.12 Storage / Persistence (2 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 55 | Storage quota normalization | `third_party/blink/renderer/modules/quota/storage_manager.cc` | `storage.estimate()` → ~500MB |
| 56 | StorageBuckets API normalization | `third_party/blink/renderer/modules/storage_buckets/*.cc` | Closes last incognito detection vector |

### 2.13 Miscellaneous (2 patches)

| # | Patch | Chromium File | Behavior |
|---|-------|--------------|----------|
| 57 | WebAuthn capabilities | `device/fido/*.cc`, `content/browser/webauth/*.cc` | WebAuthn authenticator spoofing |
| 58 | Native locale spoofing | `third_party/blink/renderer/core/frame/local_dom_window.cc` | Source-level locale (not CDP emulation) |

---

## 3. Architecture

### 3.1 PureCloakSeed Singleton

A thread-safe singleton provides per-process fingerprint configuration to all patch points:

```cpp
// purecloak/common/purecloak_seed.h
class PureCloakSeed {
 public:
  static PureCloakSeed* GetInstance();

  // Called during browser startup from command-line parsing
  void InitializeFromCommandLine(const base::CommandLine& cmd);

  // Per-flag accessors
  int seed() const;
  const std::string& platform() const;
  const std::string& gpu_vendor() const;
  const std::string& gpu_renderer() const;
  int hardware_concurrency() const;
  int device_memory() const;
  int screen_width() const;
  int screen_height() const;
  int taskbar_height() const;
  const std::string& timezone() const;
  const std::string& locale() const;
  const std::string& brand() const;
  const std::string& brand_version() const;
  const std::string& platform_version() const;
  const std::string& webrtc_ip() const;
  const std::string& fonts_dir() const;
  const std::string& location() const;
  int storage_quota_mb() const;
  bool noise_enabled() const;

 private:
  int seed_ = 0;
  std::string platform_;
  std::string gpu_vendor_;
  std::string gpu_renderer_;
  int hardware_concurrency_ = 8;
  int device_memory_ = 8;
  int screen_width_ = 1920;
  int screen_height_ = 1080;
  int taskbar_height_ = 0;
  std::string timezone_;
  std::string locale_;
  std::string brand_;
  std::string brand_version_;
  std::string platform_version_;
  std::string webrtc_ip_;
  std::string fonts_dir_;
  std::string location_;
  int storage_quota_mb_ = 500;
  bool noise_enabled_ = true;
  mutable base::Lock lock_;
};
```

### 3.2 `--fingerprint-*` Flag Parsing

All patches activate via command-line flags registered in `purecloak/common/purecloak_switches.cc`:

```cpp
namespace purecloak {
namespace switches {
const char kFingerprint[] = "fingerprint";
const char kFingerprintPlatform[] = "fingerprint-platform";
const char kFingerprintGpuVendor[] = "fingerprint-gpu-vendor";
const char kFingerprintGpuRenderer[] = "fingerprint-gpu-renderer";
const char kFingerprintHardwareConcurrency[] = "fingerprint-hardware-concurrency";
const char kFingerprintDeviceMemory[] = "fingerprint-device-memory";
const char kFingerprintScreenWidth[] = "fingerprint-screen-width";
const char kFingerprintScreenHeight[] = "fingerprint-screen-height";
const char kFingerprintTaskbarHeight[] = "fingerprint-taskbar-height";
const char kFingerprintBrand[] = "fingerprint-brand";
const char kFingerprintBrandVersion[] = "fingerprint-brand-version";
const char kFingerprintPlatformVersion[] = "fingerprint-platform-version";
const char kFingerprintWebrtcIp[] = "fingerprint-webrtc-ip";
const char kFingerprintFontsDir[] = "fingerprint-fonts-dir";
const char kFingerprintLocation[] = "fingerprint-location";
const char kFingerprintTimezone[] = "fingerprint-timezone";
const char kFingerprintLocale[] = "fingerprint-locale";
const char kFingerprintStorageQuota[] = "fingerprint-storage-quota";
const char kFingerprintNoise[] = "fingerprint-noise";
}  // namespace switches
}  // namespace purecloak
```

### 3.3 Build Flag Integration

```gn
# purecloak/common/BUILD.gn
purecloak_buildflag_header("buildflags") {
  header = "common/buildflags.h"
  flags = [
    "IS_PURECLOAK=$is_purecloak",
  ]
}
```

Every patch point in Chromium source uses:
```cpp
#if BUILDFLAG(IS_PURECLOAK)
  // Patched behavior
#endif
```

---

## 4. Relationship to Existing CDP-based Protection

| Feature | Current (CDP/JS) | Target (C++) | Migration |
|---------|-----------------|--------------|-----------|
| `navigator.webdriver` | JS override | C++ patch #1 | Remove JS when C++ lands |
| Canvas noise | JS (GenerateCanvasProtector) | C++ patch #6-10 | Remove JS when C++ lands |
| WebGL vendor/renderer | JS (GenerateWebGLProtector) | C++ patch #11-12 | Remove JS when C++ lands |
| Audio noise | JS (GenerateAudioProtector) | C++ patch #19-21 | Remove JS when C++ lands |
| Font noise | JS (GenerateFontProtector) | C++ patch #23-26 | Keep JS as fallback |
| WebRTC | `--force-webrtc-ip-handling-policy` | C++ patch #49-51 | Already C++ level |
| Screen/UA overrides | CDP Emulation.* + JS | C++ patch #34-39, 44-48 | CDP removed when C++ lands |
| Timezone/Locale | CDP Emulation.setTimezoneOverride | C++ patch #58 | CDP removed when C++ lands |
| Proxy | `--proxy-server` flag | C++ patch #52-54 | Already in BuildCommandLine |

---

## 5. Platform Support

| Platform | CloakBrowser Patches | PureCloak Initial |
|----------|---------------------|-------------------|
| Linux x86_64 | 58 | ✅ 58 |
| Linux arm64 | 58 | Future |
| Windows x64 | 58 | Future |
| macOS arm64 | 26 | Future |
| macOS x64 | 26 | Future |

---

## 6. Testing

### 6.1 C++ Unit Tests
- PureCloakSeed parsing and defaults
- Each patch's behavior with/without IS_PURECLOAK flag

### 6.2 Automated Detection Tests
```bash
# Pixelscan: navigator.webdriver, WebGL, canvas, audio
# CreepJS: full fingerprint consistency
# BrowserLeaks: WebRTC IP leak, headers
# FingerprintJS: storage quota, device memory
```

### 6.3 Manual Verification
```bash
# Launch with specific fingerprint
./out/purecloak/chrome --fingerprint=12345 --fingerprint-platform=windows

# Verify via CDP
python -c "
import json, urllib.request
data = urllib.request.urlopen('http://127.0.0.1:9333/json/version').read()
print(json.loads(data))
"
```

---

## 7. Implementation Order

| Batch | Patches | Category | Effort | Impact |
|-------|---------|----------|--------|--------|
| 1 | #1-5 | Automation Signal Removal | ★★ | Primary detection vectors |
| 2 | #6-10, #19-22 | Canvas + Audio | ★★★★ | Core fingerprint diversity |
| 3 | #11-18 | WebGL | ★★★ | GPU fingerprint diversity |
| 4 | #40-48 | Hardware + UA | ★★ | navigator.* diversity |
| 5 | #34-39 | Screen/Display | ★★ | Screen fingerprint diversity |
| 6 | #23-26, #55-56 | Font + Storage | ★★★ | Font enumeration + quota |
| 7 | #49-54 | WebRTC + Network | ★★★★ | IP leak prevention |
| 8 | #27-33, #57-58 | WebGPU + Misc | ★★★ | Edge case coverage |

---

## 8. Files to Create/Modify

### New PureCloak files
```
purecloak/common/purecloak_seed.h         # PureCloakSeed singleton
purecloak/common/purecloak_seed.cc
purecloak/common/purecloak_switches.h      # --fingerprint-* flag constants
purecloak/common/purecloak_switches.cc
purecloak/common/BUILD.gn                 # buildflags target
purecloak/common/purecloak_seed_unittest.cc  # Unit tests
```

### Chromium files to modify (58 patches across ~40 files)
```
third_party/blink/renderer/modules/webdriver/navigator_webdriver.cc
content/browser/browser_main_loop.cc
content/browser/devtools/devtools_agent_host.cc
content/browser/renderer_host/input/*.cc
ui/events/blink/*.cc
third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.cc
third_party/blink/renderer/modules/canvas/canvas2d/image_data.cc
third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
gpu/command_buffer/service/gles2_cmd_decoder.cc
gpu/config/gpu_info_collector.cc
gpu/config/gpu_info_collector_win.cc
gpu/config/software_rendering_list.json
third_party/blink/renderer/modules/webaudio/audio_context.cc
third_party/blink/renderer/modules/webaudio/offline_audio_context.cc
media/filters/aac_audio_encoder.cc
third_party/blink/renderer/platform/fonts/font_cache.cc
third_party/blink/renderer/core/frame/screen.cc
third_party/blink/renderer/core/frame/local_dom_window.cc
third_party/blink/renderer/core/frame/navigator.cc
third_party/dawn/src/dawn/native/Adapter.cc
gpu/config/gpu_blocklist.xml
chrome/browser/chrome_content_browser_client.cc
chrome/browser/ui/browser_window.cc
components/embedder_support/user_agent_utils.cc
content/browser/browser_main.cc
net/url_request/url_request.cc
third_party/webrtc/pc/peer_connection.cc
third_party/webrtc/p2p/base/port.cc
net/socket/socks5_client_socket.cc
net/http/http_network_transaction.cc
net/dns/host_resolver.cc
net/http/http_request_headers.cc
third_party/blink/renderer/modules/quota/storage_manager.cc
third_party/blink/renderer/modules/storage_buckets/*.cc
device/fido/*.cc
content/browser/webauth/*.cc
chrome/browser/gpu/*.cc
```
