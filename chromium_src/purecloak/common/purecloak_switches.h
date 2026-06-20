// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_COMMON_PURECLOAK_SWITCHES_H_
#define PURECLOAK_COMMON_PURECLOAK_SWITCHES_H_

namespace purecloak {
namespace switches {

// Port for the PureCloak REST API server. Default: 9334.
extern const char kPureCloakApiPort[];

// Fingerprint seed value. Use --fingerprint=0 to auto-generate per workspace.
extern const char kFingerprint[];

// Individual fingerprint overrides (used by PureCloakSeed).
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

}  // namespace switches
}  // namespace purecloak

#endif  // PURECLOAK_COMMON_PURECLOAK_SWITCHES_H_
