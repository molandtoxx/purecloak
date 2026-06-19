// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/profiles/workspace.h"

#include <algorithm>
#include <random>

#include "base/uuid.h"
#include "base/values.h"

namespace purecloak {

namespace {

constexpr char kIdKey[] = "id";
constexpr char kNameKey[] = "name";
constexpr char kTypeKey[] = "type";
constexpr char kColorKey[] = "color";
constexpr char kDefaultTabTitleKey[] = "default_tab_title";
constexpr char kNotesKey[] = "notes";
constexpr char kFingerprintSeedKey[] = "fingerprint_seed";
constexpr char kUserAgentKey[] = "user_agent";
constexpr char kScreenWidthKey[] = "screen_width";
constexpr char kScreenHeightKey[] = "screen_height";
constexpr char kGpuVendorKey[] = "gpu_vendor";
constexpr char kGpuRendererKey[] = "gpu_renderer";
constexpr char kHardwareConcurrencyKey[] = "hardware_concurrency";
constexpr char kPlatformKey[] = "platform";
constexpr char kColorSchemeKey[] = "color_scheme";
constexpr char kProxyKey[] = "proxy";
constexpr char kTimezoneKey[] = "timezone";
constexpr char kLocaleKey[] = "locale";
constexpr char kGeoipKey[] = "geoip";
constexpr char kHumanizeKey[] = "humanize";
constexpr char kHumanPresetKey[] = "human_preset";
constexpr char kHeadlessKey[] = "headless";
constexpr char kClipboardSyncKey[] = "clipboard_sync";
constexpr char kLaunchArgsKey[] = "launch_args";
constexpr char kAutoLaunchKey[] = "auto_launch";
constexpr char kTagKey[] = "tag";
constexpr char kTagColorKey[] = "color";
constexpr char kTagsKey[] = "tags";
constexpr char kCreatedAtKey[] = "created_at";
constexpr char kUpdatedAtKey[] = "updated_at";

// Serialize a base::Time to ISO 8601 string.
std::string TimeToString(base::Time t) {
  if (t.is_null()) {
    return std::string();
  }
  base::Time::Exploded exploded;
  t.UTCExplode(&exploded);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           exploded.year, exploded.month, exploded.day_of_month, exploded.hour,
           exploded.minute, exploded.second, exploded.millisecond);
  return std::string(buf);
}

// Parse an ISO 8601 string to base::Time.
base::Time TimeFromString(const std::string& str) {
  if (str.empty()) {
    return base::Time();
  }
  base::Time t;
  if (!base::Time::FromUTCString(str.c_str(), &t)) {
    std::string spaced = str;
    auto pos = spaced.find('T');
    if (pos != std::string::npos) {
      spaced[pos] = ' ';
      if (base::Time::FromUTCString(spaced.c_str(), &t)) {
        return t;
      }
    }
  }
  return t;
}

}  // namespace

// --- ProfileTag ---

base::DictValue ProfileTag::ToDict() const {
  base::DictValue dict;
  dict.Set(kTagKey, tag);
  dict.Set(kTagColorKey, color);
  return dict;
}

ProfileTag ProfileTag::FromDict(const base::DictValue& dict) {
  ProfileTag result;
  if (const std::string* v = dict.FindString(kTagKey)) {
    result.tag = *v;
  }
  if (const std::string* c = dict.FindString(kTagColorKey)) {
    result.color = *c;
  }
  return result;
}

// --- Workspace serialization ---

base::DictValue Workspace::ToDict() const {
  base::DictValue dict;

  // Identity
  dict.Set(kIdKey, id);
  dict.Set(kNameKey, name);
  dict.Set(kTypeKey, TypeToString(type));
  dict.Set(kColorKey, static_cast<int>(color));
  if (!default_tab_title.empty()) {
    dict.Set(kDefaultTabTitleKey, default_tab_title);
  }
  if (!notes.empty()) {
    dict.Set(kNotesKey, notes);
  }

  // Fingerprint
  dict.Set(kFingerprintSeedKey, fingerprint_seed);
  if (!user_agent.empty()) {
    dict.Set(kUserAgentKey, user_agent);
  }
  dict.Set(kScreenWidthKey, screen_width);
  dict.Set(kScreenHeightKey, screen_height);
  if (!gpu_vendor.empty()) {
    dict.Set(kGpuVendorKey, gpu_vendor);
  }
  if (!gpu_renderer.empty()) {
    dict.Set(kGpuRendererKey, gpu_renderer);
  }
  if (hardware_concurrency != 0) {
    dict.Set(kHardwareConcurrencyKey, hardware_concurrency);
  }
  if (!platform.empty()) {
    dict.Set(kPlatformKey, platform);
  }
  if (!color_scheme.empty()) {
    dict.Set(kColorSchemeKey, color_scheme);
  }

  // Network
  if (!proxy.empty()) {
    dict.Set(kProxyKey, proxy);
  }
  if (!timezone.empty()) {
    dict.Set(kTimezoneKey, timezone);
  }
  if (!locale.empty()) {
    dict.Set(kLocaleKey, locale);
  }
  dict.Set(kGeoipKey, geoip);

  // Behavior
  dict.Set(kHumanizeKey, humanize);
  if (!human_preset.empty()) {
    dict.Set(kHumanPresetKey, human_preset);
  }
  dict.Set(kHeadlessKey, headless);
  dict.Set(kClipboardSyncKey, clipboard_sync);
  dict.Set(kAutoLaunchKey, auto_launch);

  // Launch args
  base::ListValue launch_args_list;
  for (const auto& arg : launch_args) {
    launch_args_list.Append(arg);
  }
  dict.Set(kLaunchArgsKey, std::move(launch_args_list));

  // Tags
  base::ListValue tags_list;
  for (const auto& tag : tags) {
    tags_list.Append(tag.ToDict());
  }
  dict.Set(kTagsKey, std::move(tags_list));

  // Metadata
  dict.Set(kCreatedAtKey, TimeToString(created_at));
  dict.Set(kUpdatedAtKey, TimeToString(updated_at));

  return dict;
}

Workspace Workspace::FromDict(const base::DictValue& dict) {
  Workspace ws;

  // Identity
  if (const std::string* v = dict.FindString(kIdKey)) {
    ws.id = *v;
  }
  if (const std::string* v = dict.FindString(kNameKey)) {
    ws.name = *v;
  }
  if (const std::string* v = dict.FindString(kTypeKey)) {
    ws.type = StringToType(*v);
  }
  if (std::optional<int> v = dict.FindInt(kColorKey)) {
    ws.color = static_cast<uint32_t>(*v);
  }
  if (const std::string* v = dict.FindString(kDefaultTabTitleKey)) {
    ws.default_tab_title = *v;
  }
  if (const std::string* v = dict.FindString(kNotesKey)) {
    ws.notes = *v;
  }

  // Fingerprint
  if (std::optional<int> v = dict.FindInt(kFingerprintSeedKey)) {
    ws.fingerprint_seed = *v;
  }
  if (const std::string* v = dict.FindString(kUserAgentKey)) {
    ws.user_agent = *v;
  }
  if (std::optional<int> v = dict.FindInt(kScreenWidthKey)) {
    ws.screen_width = *v;
  }
  if (std::optional<int> v = dict.FindInt(kScreenHeightKey)) {
    ws.screen_height = *v;
  }
  if (const std::string* v = dict.FindString(kGpuVendorKey)) {
    ws.gpu_vendor = *v;
  }
  if (const std::string* v = dict.FindString(kGpuRendererKey)) {
    ws.gpu_renderer = *v;
  }
  if (std::optional<int> v = dict.FindInt(kHardwareConcurrencyKey)) {
    ws.hardware_concurrency = *v;
  }
  if (const std::string* v = dict.FindString(kPlatformKey)) {
    ws.platform = *v;
  }
  if (const std::string* v = dict.FindString(kColorSchemeKey)) {
    ws.color_scheme = *v;
  }

  // Network
  if (const std::string* v = dict.FindString(kProxyKey)) {
    ws.proxy = *v;
  }
  if (const std::string* v = dict.FindString(kTimezoneKey)) {
    ws.timezone = *v;
  }
  if (const std::string* v = dict.FindString(kLocaleKey)) {
    ws.locale = *v;
  }
  if (std::optional<bool> v = dict.FindBool(kGeoipKey)) {
    ws.geoip = *v;
  }

  // Behavior
  if (std::optional<bool> v = dict.FindBool(kHumanizeKey)) {
    ws.humanize = *v;
  }
  if (const std::string* v = dict.FindString(kHumanPresetKey)) {
    ws.human_preset = *v;
  }
  if (std::optional<bool> v = dict.FindBool(kHeadlessKey)) {
    ws.headless = *v;
  }
  if (std::optional<bool> v = dict.FindBool(kClipboardSyncKey)) {
    ws.clipboard_sync = *v;
  }
  if (std::optional<bool> v = dict.FindBool(kAutoLaunchKey)) {
    ws.auto_launch = *v;
  }

  // Launch args
  if (const base::ListValue* list = dict.FindList(kLaunchArgsKey)) {
    for (const auto& item : *list) {
      if (item.is_string()) {
        ws.launch_args.push_back(item.GetString());
      }
    }
  }

  // Tags
  if (const base::ListValue* list = dict.FindList(kTagsKey)) {
    for (const auto& item : *list) {
      if (item.is_dict()) {
        ws.tags.push_back(ProfileTag::FromDict(item.GetDict()));
      }
    }
  }

  // Metadata
  if (const std::string* v = dict.FindString(kCreatedAtKey)) {
    ws.created_at = TimeFromString(*v);
  }
  if (const std::string* v = dict.FindString(kUpdatedAtKey)) {
    ws.updated_at = TimeFromString(*v);
  }

  return ws;
}

// static
std::string Workspace::GenerateId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// static
int Workspace::GenerateSeed() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(10000, 99999);
  return dist(gen);
}

// static
Workspace Workspace::CreateBasic(const std::string& name, Type type) {
  Workspace ws;
  ws.id = GenerateId();
  ws.name = name;
  ws.type = type;
  ws.fingerprint_seed = GenerateSeed();
  ws.screen_width = 1920;
  ws.screen_height = 1080;
  ws.platform = "windows";
  ws.clipboard_sync = true;
  ws.human_preset = "default";
  ws.color = 0xFF6366F1;
  base::Time now = base::Time::Now();
  ws.created_at = now;
  ws.updated_at = now;
  return ws;
}

// static
const char* Workspace::TypeToString(Type type) {
  switch (type) {
    case Type::kNormal:
      return "normal";
    case Type::kFingerprint:
      return "fingerprint";
  }
}

// static
Workspace::Type Workspace::StringToType(const std::string& str) {
  if (str == "fingerprint") {
    return Type::kFingerprint;
  }
  return Type::kNormal;
}

}  // namespace purecloak
