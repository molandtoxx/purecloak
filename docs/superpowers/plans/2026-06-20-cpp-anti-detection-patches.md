# PureCloak C++ Anti-Detection Patches — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox syntax.

**Goal:** Port all 58 C++ anti-detection patches from CloakBrowser to PureCloak's Chromium fork, replacing CDP/JS-level fingerprint injection with native C++ patches.

**Architecture:** A `PureCloakSeed` singleton reads `--fingerprint-*` command-line flags at startup and provides deterministic values to 58 `#if BUILDFLAG(IS_PURECLOAK)` guarded patch points across ~40 Chromium source files spanning Blink, GPU, Network, and Content layers.

**Tech Stack:** C++20, Chromium Blink/V8, GPU/WebGL, WebAudio, Network stack, WebRTC.

**Design Doc:** `docs/design/2026-06-20-cpp-anti-detection-patches.md`

---

## File Structure

## Batch 0: PureCloakSeed Infrastructure

**Files:**
- Create: `src/purecloak/common/purecloak_seed.h`
- Create: `src/purecloak/common/purecloak_seed.cc`
- Create: `src/purecloak/common/purecloak_seed_unittest.cc`
- Create/Modify: `src/purecloak/common/BUILD.gn`
- Modify: `src/purecloak/common/purecloak_switches.h` (add `--fingerprint-*` flags)
- Modify: `src/purecloak/common/purecloak_switches.cc`

### Step 1: Create purecloak_seed.h

```cpp
#ifndef PURECLOAK_COMMON_PURECLOAK_SEED_H_
#define PURECLOAK_COMMON_PURECLOAK_SEED_H_

#include <string>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace purecloak {

// Thread-safe singleton providing per-process fingerprint configuration.
// Populated from --fingerprint-* command-line flags during browser startup.
// All Blink/GPU/Network patch points access this to get deterministic seed values.
class PureCloakSeed {
 public:
  static PureCloakSeed* GetInstance();

  // Initialize from command-line flags. Called once during startup.
  void InitializeFromCommandLine(const base::CommandLine& cmd);

  // Accessors (all thread-safe, return const refs or values)
  int seed() const;
  const std::string& platform() const;
  const std::string& gpu_vendor() const;
  const std::string& gpu_renderer() const;
  int hardware_concurrency() const;
  int device_memory() const;
  int screen_width() const;
  int screen_height() const;
  int taskbar_height() const;
  const std::string& brand() const;
  const std::string& brand_version() const;
  const std::string& platform_version() const;
  const std::string& webrtc_ip() const;
  const std::string& fonts_dir() const;
  const std::string& location() const;
  int storage_quota_mb() const;
  bool noise_enabled() const;

 private:
  friend class base::NoDestructor<PureCloakSeed>;
  PureCloakSeed() = default;
  ~PureCloakSeed() = default;

  // Platform-aware default helpers
  static int DefaultScreenWidth(const std::string& platform);
  static int DefaultScreenHeight(const std::string& platform);
  static int DefaultTaskbarHeight(const std::string& platform);
  static int DefaultHardwareConcurrency(const std::string& platform);

  mutable base::Lock lock_;

  int seed_ = 0;
  std::string platform_;
  std::string gpu_vendor_;
  std::string gpu_renderer_;
  int hardware_concurrency_ = 8;
  int device_memory_ = 8;
  int screen_width_ = 1920;
  int screen_height_ = 1080;
  int taskbar_height_ = 0;
  std::string brand_;
  std::string brand_version_;
  std::string platform_version_;
  std::string webrtc_ip_;
  std::string fonts_dir_;
  std::string location_;
  int storage_quota_mb_ = 500;
  bool noise_enabled_ = true;
};

}  // namespace purecloak

#endif  // PURECLOAK_COMMON_PURECLOAK_SEED_H_
```

### Step 2: Create purecloak_seed.cc

```cpp
#include "purecloak/common/purecloak_seed.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "purecloak/common/purecloak_switches.h"

namespace purecloak {

// static
PureCloakSeed* PureCloakSeed::GetInstance() {
  static base::NoDestructor<PureCloakSeed> instance;
  return instance.get();
}

void PureCloakSeed::InitializeFromCommandLine(const base::CommandLine& cmd) {
  base::AutoLock lock(lock_);

  // Helper to read switch value or return default
  auto get_switch = [&](const char* name, const std::string& default_val) {
    return cmd.HasSwitch(name) ? cmd.GetSwitchValueASCII(name) : default_val;
  };

  seed_ = 0;
  std::string seed_str = cmd.GetSwitchValueASCII(switches::kFingerprint);
  if (!seed_str.empty()) {
    base::StringToInt(seed_str, &seed_);
  }

  platform_ = get_switch(switches::kFingerprintPlatform, "windows");
  gpu_vendor_ = get_switch(switches::kFingerprintGpuVendor, "");
  gpu_renderer_ = get_switch(switches::kFingerprintGpuRenderer, "");

  int tmp;
  hw_concurrency_str_ = get_switch(switches::kFingerprintHardwareConcurrency, "");
  if (base::StringToInt(hw_concurrency_str_, &tmp)) hardware_concurrency_ = tmp;
  else hardware_concurrency_ = DefaultHardwareConcurrency(platform_);

  screen_width_ = DefaultScreenWidth(platform_);
  screen_height_ = DefaultScreenHeight(platform_);
  taskbar_height_ = DefaultTaskbarHeight(platform_);

  std::string sw = get_switch(switches::kFingerprintScreenWidth, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) screen_width_ = tmp;
  sw = get_switch(switches::kFingerprintScreenHeight, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) screen_height_ = tmp;
  sw = get_switch(switches::kFingerprintTaskbarHeight, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) taskbar_height_ = tmp;

  brand_ = get_switch(switches::kFingerprintBrand, "");
  brand_version_ = get_switch(switches::kFingerprintBrandVersion, "");
  platform_version_ = get_switch(switches::kFingerprintPlatformVersion, "");
  webrtc_ip_ = get_switch(switches::kFingerprintWebrtcIp, "");
  fonts_dir_ = get_switch(switches::kFingerprintFontsDir, "");
  location_ = get_switch(switches::kFingerprintLocation, "");

  sw = get_switch(switches::kFingerprintStorageQuota, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) storage_quota_mb_ = tmp;

  noise_enabled_ = cmd.GetSwitchValueASCII(switches::kFingerprintNoise) != "false";
}

// Platform-aware defaults
int PureCloakSeed::DefaultScreenWidth(const std::string& platform) {
  return platform == "macos" ? 1440 : 1920;
}

int PureCloakSeed::DefaultScreenHeight(const std::string& platform) {
  return platform == "macos" ? 900 : 1080;
}

int PureCloakSeed::DefaultTaskbarHeight(const std::string& platform) {
  if (platform == "windows") return 48;
  if (platform == "macos") return 95;
  return 0; // Linux
}

int PureCloakSeed::DefaultHardwareConcurrency(const std::string& platform) {
  return 8; // modern baseline
}
```

### Step 3: Add switches

Add to `purecloak_switches.h`:
```cpp
extern const char kFingerprint[];
extern const char kFingerprintPlatform[];
extern const char kFingerprintGpuVendor[];
extern const char kFingerprintGpuRenderer[];
extern const char kFingerprintHardwareConcurrency[];
extern const char kFingerprintDeviceMemory[];
extern const char kFingerprintScreenWidth[];
extern const char kFingerprintScreenHeight[];
extern const char kFingerprintTaskbarHeight[];
extern const char kFingerprintBrand[];
extern const char kFingerprintBrandVersion[];
extern const char kFingerprintPlatformVersion[];
extern const char kFingerprintWebrtcIp[];
extern const char kFingerprintFontsDir[];
extern const char kFingerprintLocation[];
extern const char kFingerprintStorageQuota[];
extern const char kFingerprintNoise[];
```

### Step 4: Create common BUILD.gn

```python
import("//purecloak/purecloak.gni")

assert(is_purecloak)

purecloak_buildflag_header("buildflags") {
  header = "common/buildflags.h"
  flags = [ "IS_PURECLOAK=$is_purecloak" ]
}

source_set("common") {
  sources = [
    "purecloak_seed.cc",
    "purecloak_seed.h",
    "purecloak_switches.cc",
    "purecloak_switches.h",
  ]
  deps = [
    ":buildflags",
    "//base",
  ]
}

source_set("unit_tests") {
  testonly = true
  sources = [ "purecloak_seed_unittest.cc" ]
  deps = [
    ":common",
    "//base/test:test_support",
    "//testing/gtest",
  ]
}
```

## Batch 1: Automation Signal Removal (Patches #1-5)

### Patch #1: navigator.webdriver → false

**File:** `src/third_party/blink/renderer/core/frame/navigator.cc`

Around line 100:
```cpp
bool Navigator::webdriver() const {
#if BUILDFLAG(IS_PURECLOAK)
  return false;
#else
  return Controller().IsWindow();
#endif
}
```

Also need to add the include:
```cpp
#if BUILDFLAG(IS_PURECLOAK)
#include "purecloak/common/buildflags.h"
#endif
```

### Patch #2: --enable-automation suppression

**File:** `src/content/browser/browser_main_loop.cc`

Find where `--enable-automation` is checked and short-circuit:
```cpp
void BrowserMainLoop::PreMainMessageLoopRun() {
#if BUILDFLAG(IS_PURECLOAK)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    // Suppress all automation markers in PureCloak builds
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableAutomation);
  }
#endif
  // ... existing code
}
```

### Patch #3: CDP detection removal

**File:** `src/content/browser/devtools/devtools_agent_host.cc`

Find where `isAutomatedWithCDP` is set:
```cpp
#if BUILDFLAG(IS_PURECLOAK)
  // Never report CDP automation
  response.Set("isAutomatedWithCDP", false);
#else
  // existing code
#endif
```

### Patch #4: CDP input stealth

**Files:** Various `content/browser/renderer_host/input/` files.

This is complex and spans multiple files. The key idea: when input events originate from CDP, don't set the `from_web_test` or automation flags on the events themselves.

### Patch #5: Other automation marker reduction

Similar pattern in various Chrome/Content files.

## Remaining Batches (2-8)

Follow the same pattern for batches 2-8 as defined in the design doc. Each batch follows the same structure:
1. Find the Chromium source file
2. Add `#if BUILDFLAG(IS_PURECLOAK)` guard
3. Include "purecloak/common/buildflags.h" and "purecloak/common/purecloak_seed.h"
4. Add patched behavior using PureCloakSeed::GetInstance() accessors
5. Write unit test for the specific patch

## Commit Pattern

Each patch should be committed independently:
```bash
git add src/third_party/blink/renderer/core/frame/navigator.cc
git commit -m "feat(stealth): patch navigator.webdriver to always return false"
```
