// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_PROFILES_WORKSPACE_H_
#define PURECLOAK_BROWSER_PROFILES_WORKSPACE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"

namespace purecloak {

// A single profile tag (label + color).
struct ProfileTag {
  std::string tag;          // tag text
  std::string color;        // hex color e.g. "#6366f1"

  base::DictValue ToDict() const;
  static ProfileTag FromDict(const base::DictValue& dict);
};

// A PureCloak workspace: an isolated browsing environment.
//
// Each Workspace maps to one PureCloak subprocess with its own --user-data-dir,
// proxy, fingerprint, and storage isolation. Previously the model had
// Workspace(1) → Profile(N), but after the merge, each Workspace carries
// the full configuration directly — a Workspace IS a profile.
//
// Fields are grouped by category: Identity, Fingerprint, Network, Behavior,
// Tags, Runtime (not persisted), and Metadata.
struct Workspace {
  enum class Type { kNormal, kFingerprint };

  // === Identity ===
  std::string id;                 // UUID v4, auto-generated
  std::string name;               // display name
  Type type = Type::kNormal;      // immutable after creation
  uint32_t color = 0xFF6366F1;    // ARGB color for tab/sidebar indicator
  std::string default_tab_title;  // default title for new tabs
  std::string notes;              // user notes

  // === Fingerprint ===
  int fingerprint_seed = 0;         // 10000-99999, 0 = random on create
  std::string user_agent;           // empty = auto (from binary)
  int screen_width = 1920;
  int screen_height = 1080;
  std::string gpu_vendor;           // e.g. "Google Inc. (NVIDIA)"
  std::string gpu_renderer;         // e.g. "ANGLE (NVIDIA, ...)"
  int hardware_concurrency = 0;     // 0 = auto from seed
  std::string platform;             // "windows", "macos", "linux"
  std::string color_scheme;         // "light", "dark", "no-preference", ""

  // === Network ===
  std::string proxy;                // "http://user:pass@host:port"
  std::string timezone;             // e.g. "America/New_York"
  std::string locale;               // e.g. "en-US"
  bool geoip = false;               // auto-detect tz/locale from proxy IP

  // === Behavior ===
  bool humanize = false;
  std::string human_preset;         // "default" or "careful"
  bool headless = false;
  bool clipboard_sync = true;
  std::vector<std::string> launch_args;  // extra PureCloak flags
  bool auto_launch = false;              // launch on start

  // === Tags ===
  std::vector<ProfileTag> tags;

  // === Runtime (populated at launch time, not persisted) ===
  std::string user_data_dir;  // PureCloak profile directory
  std::string status;         // "stopped", "running"
  std::string cdp_url;        // CDP endpoint when running

  // === Metadata ===
  base::Time created_at;
  base::Time updated_at;

  // Serialization to/from JSON Value.
  base::DictValue ToDict() const;
  static Workspace FromDict(const base::DictValue& dict);

  // Helpers.
  static std::string GenerateId();
  static int GenerateSeed();
  static Workspace CreateBasic(const std::string& name, Type type);
  static const char* TypeToString(Type type);
  static Type StringToType(const std::string& str);
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_PROFILES_WORKSPACE_H_
