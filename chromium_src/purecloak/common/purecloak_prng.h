// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_COMMON_PURECLOAK_PRNG_H_
#define PURECLOAK_COMMON_PURECLOAK_PRNG_H_

#include <stdint.h>

namespace purecloak {

// Tag type for disambiguating PureCloak noise injection overloads.
struct PureCloakNoiseTag {};

// Inline deterministic PRNG for canvas/audio noise injection.
// Same seed + same index => same output across runs and platforms.
// No dependencies beyond <stdint.h> — safe to include from any target.

// Returns a noise delta in range [-128, 127] (255 possible values)
// for byte-level noise (e.g., canvas pixel RGB channels).
inline int PureCloakNoise(int seed, int index) {
  uint32_t x = static_cast<uint32_t>(static_cast<uint32_t>(seed) * 0x9E3779B9u +
                                     static_cast<uint32_t>(index));
  uint32_t y = ((x ^ (x >> 16)) * 0x85EBCA6Bu) & 0xFFFFFFFFu;
  uint32_t z = ((y ^ (y >> 13)) * 0xC2B2AE35u) & 0xFFFFFFFFu;
  return static_cast<int>((z & 0xFFu)) - 128;
}

// Returns a float noise delta in range [-0.0001, 0.0001] for float audio data.
inline float PureCloakFloatNoise(int seed, int index) {
  return (static_cast<float>(PureCloakNoise(seed, index)) / 256.0f) * 0.0001f;
}

// Returns a small float noise delta in range [-0.5, 0.5] for frequency data.
inline float PureCloakFreqNoise(int seed, int index) {
  return (static_cast<float>(PureCloakNoise(seed, index)) / 256.0f) * 0.5f;
}

}  // namespace purecloak

#endif  // PURECLOAK_COMMON_PURECLOAK_PRNG_H_
