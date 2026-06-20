# PureCloak C++ Anti-Detection Patches — Design Spec

**Date**: 2026-06-20
**Status**: Draft
**Phase**: Phase 1 (P0)

---

## 1. Motivation

PureCloak currently injects anti-detection scripts via CDP `Page.addScriptToEvaluateOnNewDocument`. This JS-level approach is detectable:

| Detection Vector | JS Injection Weakness |
|-----------------|----------------------|
| `Function.prototype.toString.call(navigator.webdriver)` | Returns `"function() { [native code] }"` but the property descriptor reveals the proxy |
| `Object.getOwnPropertyDescriptor(navigator, 'webdriver')` | Shows `get` instead of `value` |
| Canvas `toDataURL` | Wrapper can be detected via `toString` on the replaced function |
| Stack trace analysis | Internal calls to injected code appear in stack traces |

CloakBrowser solves this via 58 C++ patches at the Blink/V8/browser level. PureCloak, as a Chromium fork, should do the same.

### What's Already Done
- `--force-webrtc-ip-handling-policy=disable_non_proxied_udp` — already injected in `BuildCommandLine()`

### What Needs Work
All remaining fingerprints are currently JS-level and need to be C++ level.

---

## 2. Patch Inventory (Priority Order)

### P0 — Port Now (4 patches)

#### #1: `navigator.webdriver` → always false

**File**: `third_party/blink/renderer/modules/credentialmanagement/navigator_credentials.cc`  
Or more precisely: `third_party/blink/renderer/modules/webdriver/navigator_webdriver.cc`

**Current behavior**: Returns `true` when Chrome is started with `--enable-automation` or `--remote-debugging-port`.

**Patch**: Hardcode return value to `false` when `is_purecloak` is true.

```cpp
bool NavigatorWebDriver::webdriver(ScriptState* script_state) const {
#if BUILDFLAG(IS_PURECLOAK)
  return false;
#else
  return GetExecutionContext()->IsWindow();
#endif
}
```

**Impact**: Eliminates the most commonly checked automation vector. Cannot be detected by any JS technique.

#### #2: Canvas fingerprint noise (Blink/Skia level)

**File**: `third_party/blink/renderer/modules/canvas/canvas_rendering_context_2d.cc`

**Current approach**: JS override of `toDataURL()` / `getImageData()`.

**Patch**: Add deterministic noise to `getImageData()` at the C++ level, seeded by workspace fingerprint seed.

```cpp
// In CanvasRenderingContext2D::getImageData()
#if BUILDFLAG(IS_PURECLOAK)
if (auto* seed = PureCloakSeed::Get()) {
  AddNoiseToImageData(result, seed->value());
}
#endif
```

Noise approach: ±1 to each RGB channel on ~0.1% of pixels, deterministically derived from seed. This is the same approach used by Canvas Defender and other anti-fingerprinting tools, but at the native level where it cannot be detected.

#### #3: WebGL UNMASKED_VENDOR/RENDERER override

**File**: `third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc`

**Current approach**: JS override of `getParameter()`.

**Patch**: Intercept `UNMASKED_VENDOR_WEBGL` and `UNMASKED_RENDERER_WEBGL` at the native level.

```cpp
#if BUILDFLAG(IS_PURECLOAK)
if (pname == GL_UNMASKED_VENDOR_WEBGL || /* WebGL 1 */
    pname == GL_UNMASKED_VENDOR_WEBGL_WEBGL2) { /* WebGL 2 */
  if (auto* seed = PureCloakSeed::Get()) {
    return V8String(seed->gpu_vendor());
  }
}
if (pname == GL_UNMASKED_RENDERER_WEBGL || /* WebGL 1 */
    pname == GL_UNMASKED_RENDERER_WEBGL_WEBGL2) { /* WebGL 2 */
  if (auto* seed = PureCloakSeed::Get()) {
    return V8String(seed->gpu_renderer());
  }
}
#endif
```

#### #4: AudioContext output noise

**File**: `third_party/blink/renderer/modules/webaudio/analyser_node.cc`  
Or `third_party/blink/renderer/modules/webaudio/audio_buffer.cc`

**Current approach**: JS override of `getChannelData()`.

**Patch**: Add deterministic noise to `getChannelData()` output.

```cpp
#if BUILDFLAG(IS_PURECLOAK)
void AudioBuffer::AddFingerprintNoise(Vector<float>& channel_data) {
  if (auto* seed = PureCloakSeed::Get()) {
    // Add ±0.0001 noise to ~1% of samples, seeded deterministically
    AddDeterministicNoise(channel_data, seed->value());
  }
}
#endif
```

### P1 — Port Next (2 patches)

#### #5: `navigator.plugins` spoofing

**File**: `third_party/blink/renderer/modules/plugins/dom_plugin_array.cc`

In headless mode, `navigator.plugins` returns empty array. Patch to return a non-empty list with fake plugin entries.

#### #6: Chrome runtime detection removal

**File**: `chrome/renderer/chrome_render_frame_observer.cc`

Remove `chrome.runtime` ID that identifies the browser as Chrome, to prevent `window.chrome` runtime detection in headless.

---

## 3. Architecture: PureCloakSeed

To support C++ level fingerprint injection, a new thread-safe singleton provides fingerprint data to all patch points:

```cpp
// purecloak/common/purecloak_seed.h
class PureCloakSeed {
 public:
  static PureCloakSeed* GetInstance();
  
  // Called by WorkspaceProfileApplier when a workspace is launched
  void SetActiveSeed(int seed,
                     const std::string& gpu_vendor,
                     const std::string& gpu_renderer);
  
  int seed() const;
  const std::string& gpu_vendor() const;
  const std::string& gpu_renderer() const;
  
 private:
  int seed_ = 0;
  std::string gpu_vendor_;
  std::string gpu_renderer_;
  mutable base::Lock lock_;
};
```

This is intentionally simple — the seed is set when a workspace launches and persists for the worker process lifetime. Since each workspace runs as a separate subprocess (the current PureCloak model), there's no need for per-tab seed switching.

`BUILDFLAG(IS_PURECLOAK)` is defined via `purecloak.gni` → `buildflag_header("buildflags")`.

---

## 4. Patch Locations Summary

| # | Patch | File | Lines Changed |
|---|-------|------|-------------|
| 1 | `navigator.webdriver` | `blink/renderer/modules/webdriver/navigator_webdriver.cc` | ~5 |
| 2 | Canvas noise | `blink/renderer/modules/canvas/canvas_rendering_context_2d.cc` | ~30 |
| 3 | WebGL vendor/renderer | `blink/renderer/modules/webgl/webgl_rendering_context_base.cc` | ~20 |
| 4 | AudioContext noise | `blink/renderer/modules/webaudio/audio_buffer.cc` | ~25 |
| 5 | Plugins spoofing | `blink/renderer/modules/plugins/dom_plugin_array.cc` | ~40 |
| 6 | Chrome runtime | `chrome/renderer/chrome_render_frame_observer.cc` | ~10 |

Total: ~130 lines of C++ changes across 6 files.

---

## 5. Relationship to Existing AntiDetectionEngine

The existing `AntiDetectionEngine` (JS-level) and new C++ patches coexist:

```
navigator.webdriver   → C++ patch (P0)          ← REPLACES JS version
Canvas fingerprint    → C++ patch (P0) + JS guard ← JS becomes fallback
WebGL vendor/renderer → C++ patch (P0)          ← REPLACES JS version
AudioContext noise    → C++ patch (P0)          ← REPLACES JS version
Font enumeration      → JS only (AntiDetectionEngine) ← STAYS JS (low detection risk)
WebRTC protection     → Already C++ flag        ← ALREADY DONE
Screen/UA/nav overrides → CDP Emulation API     ← STAYS CDP (Emulation.* commands are native)
```

Once C++ patches land, the JS versions in `AntiDetectionEngine::GenerateCanvasProtector()` etc. can be removed or switched to no-ops.

---

## 6. Testing

### Automated
- `PureCloakSeed` singleton + thread safety in existing unittests
- Canvas noise: deterministic output check (same seed = same noise)
- WebGL: verify returned vendor/renderer string matches configured value

### Manual (Detection Sites)
```
Pixelscan: navigator.webdriver must be false
CreepJS:  Canvas must show noise, WebGL must show spoofed strings
BrowserLeaks: WebRTC must show configured IP handling
```

### Test Script
```python
# tests/stealth_detection_test.py — runs detection sites and checks results
```
