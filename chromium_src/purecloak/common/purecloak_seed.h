// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_COMMON_PURECLOAK_SEED_H_
#define PURECLOAK_COMMON_PURECLOAK_SEED_H_

#include <string>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace purecloak {

// Thread-safe singleton providing per-process fingerprint configuration.
// Populated from --fingerprint-* command-line flags during browser startup.
// All Blink/GPU/Network patch points access this to get deterministic,
// configurable fingerprint values.
//
// Usage (from any Chromium thread):
//   PureCloakSeed::GetInstance()->platform();
//   PureCloakSeed::GetInstance()->screen_width();
class PureCloakSeed {
 public:
  static PureCloakSeed* GetInstance();

  // Initialize from command-line flags. Called once during startup.
  void InitializeFromCommandLine(const base::CommandLine& cmd);

  // --- Thread-safe accessors ---
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
  const std::string& timezone() const;
  const std::string& locale() const;
  int storage_quota_mb() const;
  bool noise_enabled() const;

 private:
  friend class base::NoDestructor<PureCloakSeed>;
  PureCloakSeed() = default;
  ~PureCloakSeed() = default;

  // Platform-aware default helpers.
  static int DefaultScreenWidth(const std::string& platform);
  static int DefaultScreenHeight(const std::string& platform);
  static int DefaultTaskbarHeight(const std::string& platform);
  static int DefaultHardwareConcurrency(const std::string& platform);
  static int DefaultDeviceMemory(const std::string& platform);

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
  std::string timezone_;
  std::string locale_;
  int storage_quota_mb_ = 500;
  bool noise_enabled_ = true;
};

}  // namespace purecloak

#endif  // PURECLOAK_COMMON_PURECLOAK_SEED_H_
