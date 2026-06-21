// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/common/purecloak_seed.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "purecloak/common/purecloak_switches.h"

namespace purecloak {

PureCloakSeed::PureCloakSeed() = default;
PureCloakSeed::~PureCloakSeed() = default;

// static
PureCloakSeed* PureCloakSeed::GetInstance() {
  static base::NoDestructor<PureCloakSeed> instance;
  return instance.get();
}

void PureCloakSeed::InitializeFromCommandLine(const base::CommandLine& cmd) {
  base::AutoLock lock(lock_);

  auto get_switch = [&](const char* name, const std::string& default_val) {
    return cmd.HasSwitch(name) ? cmd.GetSwitchValueASCII(name) : default_val;
  };

  // Seed value (0 = auto-generate per-workspace).
  seed_ = 0;
  std::string seed_str = cmd.GetSwitchValueASCII(switches::kFingerprint);
  if (!seed_str.empty()) {
    base::StringToInt(seed_str, &seed_);
  }

  // Platform.
  platform_ =
      get_switch(switches::kFingerprintPlatform, "windows");

  // GPU.
  gpu_vendor_ = get_switch(switches::kFingerprintGpuVendor, "");
  gpu_renderer_ = get_switch(switches::kFingerprintGpuRenderer, "");

  // Hardware concurrency.
  int tmp = 0;
  std::string hw_str =
      get_switch(switches::kFingerprintHardwareConcurrency, "");
  if (!hw_str.empty() && base::StringToInt(hw_str, &tmp)) {
    hardware_concurrency_ = tmp;
  } else {
    hardware_concurrency_ = DefaultHardwareConcurrency(platform_);
  }

  // Device memory.
  std::string dm_str = get_switch(switches::kFingerprintDeviceMemory, "");
  if (!dm_str.empty() && base::StringToInt(dm_str, &tmp)) {
    device_memory_ = tmp;
  } else {
    device_memory_ = DefaultDeviceMemory(platform_);
  }

  // Screen.
  screen_width_ = DefaultScreenWidth(platform_);
  screen_height_ = DefaultScreenHeight(platform_);
  taskbar_height_ = DefaultTaskbarHeight(platform_);

  std::string sw = get_switch(switches::kFingerprintScreenWidth, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) {
    screen_width_ = tmp;
  }
  sw = get_switch(switches::kFingerprintScreenHeight, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) {
    screen_height_ = tmp;
  }
  sw = get_switch(switches::kFingerprintTaskbarHeight, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) {
    taskbar_height_ = tmp;
  }

  // Branding.
  brand_ = get_switch(switches::kFingerprintBrand, "");
  brand_version_ = get_switch(switches::kFingerprintBrandVersion, "");
  platform_version_ = get_switch(switches::kFingerprintPlatformVersion, "");
  webrtc_ip_ = get_switch(switches::kFingerprintWebrtcIp, "");
  fonts_dir_ = get_switch(switches::kFingerprintFontsDir, "");
  location_ = get_switch(switches::kFingerprintLocation, "");
  timezone_ = get_switch(switches::kFingerprintTimezone, "");
  locale_ = get_switch(switches::kFingerprintLocale, "");

  // Storage.
  sw = get_switch(switches::kFingerprintStorageQuota, "");
  if (!sw.empty() && base::StringToInt(sw, &tmp)) {
    storage_quota_mb_ = tmp;
  }

  // Noise.
  noise_enabled_ =
      cmd.GetSwitchValueASCII(switches::kFingerprintNoise) != "false";

  VLOG(1) << "PureCloakSeed initialized: platform=" << platform_
          << " screen=" << screen_width_ << "x" << screen_height_;
}

// --- Accessors ---

int PureCloakSeed::seed() const {
  base::AutoLock lock(lock_);
  return seed_;
}

const std::string& PureCloakSeed::platform() const {
  base::AutoLock lock(lock_);
  return platform_;
}

const std::string& PureCloakSeed::gpu_vendor() const {
  base::AutoLock lock(lock_);
  return gpu_vendor_;
}

const std::string& PureCloakSeed::gpu_renderer() const {
  base::AutoLock lock(lock_);
  return gpu_renderer_;
}

int PureCloakSeed::hardware_concurrency() const {
  base::AutoLock lock(lock_);
  return hardware_concurrency_;
}

int PureCloakSeed::device_memory() const {
  base::AutoLock lock(lock_);
  return device_memory_;
}

int PureCloakSeed::screen_width() const {
  base::AutoLock lock(lock_);
  return screen_width_;
}

int PureCloakSeed::screen_height() const {
  base::AutoLock lock(lock_);
  return screen_height_;
}

int PureCloakSeed::taskbar_height() const {
  base::AutoLock lock(lock_);
  return taskbar_height_;
}

const std::string& PureCloakSeed::brand() const {
  base::AutoLock lock(lock_);
  return brand_;
}

const std::string& PureCloakSeed::brand_version() const {
  base::AutoLock lock(lock_);
  return brand_version_;
}

const std::string& PureCloakSeed::platform_version() const {
  base::AutoLock lock(lock_);
  return platform_version_;
}

const std::string& PureCloakSeed::webrtc_ip() const {
  base::AutoLock lock(lock_);
  return webrtc_ip_;
}

const std::string& PureCloakSeed::fonts_dir() const {
  base::AutoLock lock(lock_);
  return fonts_dir_;
}

const std::string& PureCloakSeed::location() const {
  base::AutoLock lock(lock_);
  return location_;
}

const std::string& PureCloakSeed::timezone() const {
  base::AutoLock lock(lock_);
  return timezone_;
}

const std::string& PureCloakSeed::locale() const {
  base::AutoLock lock(lock_);
  return locale_;
}

int PureCloakSeed::storage_quota_mb() const {
  base::AutoLock lock(lock_);
  return storage_quota_mb_;
}

bool PureCloakSeed::noise_enabled() const {
  base::AutoLock lock(lock_);
  return noise_enabled_;
}

// --- Platform-aware defaults ---

// static
int PureCloakSeed::DefaultScreenWidth(const std::string& platform) {
  return platform == "macos" ? 1440 : 1920;
}

// static
int PureCloakSeed::DefaultScreenHeight(const std::string& platform) {
  return platform == "macos" ? 900 : 1080;
}

// static
int PureCloakSeed::DefaultTaskbarHeight(const std::string& platform) {
  if (platform == "windows") return 48;
  if (platform == "macos") return 95;
  return 0;  // Linux, no persistent taskbar by default.
}

// static
int PureCloakSeed::DefaultHardwareConcurrency(
    const std::string& platform) {
  // Platform-aware hardware concurrency defaults based on common real-world
  // configurations. Android devices typically have fewer cores than desktops,
  // while macOS tends toward efficiency cores.
  if (platform == "android") return 4;
  if (platform == "ios") return 4;
  // Modern baseline: 8 cores is the most common desktop fingerprint.
  return 8;
}

// static
int PureCloakSeed::DefaultDeviceMemory(const std::string& platform) {
  // Platform-aware device memory defaults based on common configurations.
  // Mobile devices typically have less RAM than desktops.
  if (platform == "android") return 4;
  if (platform == "ios") return 3;
  // Desktop baseline: 8 GB is the most common configuration.
  return 8;
}

}  // namespace purecloak
