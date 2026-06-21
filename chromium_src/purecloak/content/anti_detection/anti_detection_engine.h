// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_CONTENT_ANTI_DETECTION_ANTI_DETECTION_ENGINE_H_
#define PURECLOAK_CONTENT_ANTI_DETECTION_ANTI_DETECTION_ENGINE_H_

#include <string>

#include "base/values.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

// Generates anti-detection JavaScript injection scripts that spoof
// Canvas, WebGL, Audio, and Font fingerprinting.
//
// All scripts use seed-based determinism: same seed → same fingerprint,
// different seed → different fingerprint. Seed 0 = random (generated
// at profile creation time).
//
// Scripts are injected via Page.addScriptToEvaluateOnNewDocument so they
// execute before any page script.
class AntiDetectionEngine {
 public:
  AntiDetectionEngine() = default;
  ~AntiDetectionEngine() = default;

  // Generates the complete anti-detection injection script for a workspace.
  // Combines all protectors that are relevant to the workspace's seed.
  // Returns empty string if fingerprint_seed is 0 (no protection).
  std::string GenerateProtectionScript(const Workspace& workspace) const;

  // Individual protector script generators.
  std::string GenerateCanvasProtector(int seed) const;
  std::string GenerateWebGLProtector(const Workspace& workspace) const;
  std::string GenerateAudioProtector(int seed) const;
  std::string GenerateFontProtector(int seed) const;
  std::string GenerateWebRTCProtector() const;
  std::string GenerateWebdriverRemover() const;

  // Phase 0 — P0 anti-detection gap fixes.
  std::string GenerateHardwareConcurrencyProtector(int hw_concurrency) const;
  std::string GenerateDeviceMemoryProtector(int device_memory) const;
  std::string GenerateScreenProtector(int width, int height) const;
  std::string GenerateTimezoneProtector(const std::string& tz) const;
  std::string GeneratePlatformProtector(const std::string& platform) const;
  std::string GenerateColorSchemeProtector(const std::string& scheme) const;

  // Phase 1 — Deep anti-detection reinforcement.
  std::string GenerateFeatureConsistencyProtector(
      const std::string& platform) const;
  std::string GenerateWebGLExtensionFilter(
      const std::string& gpu_vendor) const;
  std::string GeneratePerformanceProtector() const;
  std::string GenerateGeolocationProtector(double lat, double lng) const;
  std::string GenerateBatteryProtector() const;
  std::string GenerateNetworkInfoProtector() const;

  // Generates the seed-based PRNG JavaScript function.
  // Produces deterministic noise values: same seed+index → same output.
  static std::string GeneratePRNGFunction(int seed);
};

}  // namespace purecloak

#endif  // PURECLOAK_CONTENT_ANTI_DETECTION_ANTI_DETECTION_ENGINE_H_
