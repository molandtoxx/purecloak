# Merge Profile into Workspace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Merge the separate Workspace + Profile(1:N) model into a single Workspace entity where each Workspace IS a profile with full config, enabling "1 workspace = 1 Chrome process = fully isolated"

**Architecture:** Currently Workspace is a thin container (id, name, type) with a separate Profile struct holding all config fields (fingerprint, proxy, UA, etc.) — these share a 1:N relationship. After merge, Workspace absorbs all Profile fields plus the existing type field. ProfileStore is deleted entirely. WorkspaceStore becomes the single store. WebUI, side panel, launcher, and cloakbrowser-suite are updated to match.

**Tech Stack:** C++ (Chromium Views/WebUI), Python (FastAPI/SQLite), TypeScript (React)

**Current files to modify:**
- `chromium_src/purecloak/browser/profiles/workspace.h` — add all profile fields
- `chromium_src/purecloak/browser/profiles/workspace.cc` — expand serialization
- `chromium_src/purecloak/browser/profiles/profile.h` — delete Profile struct, keep ProfileTag
- `chromium_src/purecloak/browser/profiles/profile.cc` — delete Profile serialization, keep ProfileTag
- `chromium_src/purecloak/browser/profiles/workspace_store.h` — add full-field update methods
- `chromium_src/purecloak/browser/profiles/workspace_store.cc` — implement full-field updates
- `chromium_src/purecloak/browser/profiles/profile_store.h` — DELETE
- `chromium_src/purecloak/browser/profiles/profile_store.cc` — DELETE
- `chromium_src/purecloak/browser/profiles/store_provider.h` — remove ProfileStore
- `chromium_src/purecloak/browser/profiles/store_provider.cc` — remove ProfileStore creation
- `chromium_src/purecloak/browser/ui/webui/purecloak_handler.h` — remove profile handlers
- `chromium_src/purecloak/browser/ui/webui/purecloak_handler.cc` — merge handlers
- `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.h` — remove ProfileStore
- `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.cc` — remove expandable profiles
- `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_coordinator.cc` — remove ProfileStore init
- `chromium_src/purecloak/browser/ui/views/side_panel/workspace_management_dialogs.h` — merge dialogs
- `chromium_src/purecloak/browser/ui/views/side_panel/workspace_management_dialogs.cc` — merge dialog impl
- `chromium_src/purecloak/browser/workspace_launcher.h` — remove Profile vector
- `chromium_src/purecloak/browser/workspace_launcher.cc` — simplify launch (no profiles)
- `chromium_src/purecloak/browser/workspace_profile_applier.h` — simplify
- `chromium_src/purecloak/browser/workspace_profile_applier.cc` — simplify
- `chromium_src/purecloak/content/anti_detection/anti_detection_engine.h` — update references
- `chromium_src/purecloak/content/profile_cdp_injector.h` — change Profile→Workspace refs
- `chromium_src/purecloak/content/profile_cdp_injector.cc` — change Profile→Workspace refs
- `chromium_src/purecloak/resources/purecloak.html` — remove profile UI
- `chromium_src/purecloak/browser/profiles/BUILD.gn` — remove profile_store, update deps
- `chromium_src/purecloak/resources/BUILD.gn` — remove profile JS artifacts if any
- `cloakbrowser-suite/backend/database.py` — add `type` column, adjust schema
- `cloakbrowser-suite/backend/models.py` — add `type`, remove workspace_id usage
- `cloakbrowser-suite/frontend/src/lib/api.ts` — add `type` field

**Files to create:** none (merge into existing workspace files)

**Files to delete:**
- `chromium_src/purecloak/browser/profiles/profile_store.h`
- `chromium_src/purecloak/browser/profiles/profile_store.cc`
- `chromium_src/purecloak/browser/profiles/profile.h` (after extracting ProfileTag)
- `chromium_src/purecloak/browser/profiles/profile.cc` (after extracting ProfileTag)

---

### Task 1: Merge Workspace + Profile structs

**Files:**
- Modify: `chromium_src/purecloak/browser/profiles/workspace.h`
- Modify: `chromium_src/purecloak/browser/profiles/workspace.cc`
- Modify: `chromium_src/purecloak/browser/profiles/profile.h` — extract ProfileTag, delete Profile/ProfileCollection
- Modify: `chromium_src/purecloak/browser/profiles/profile.cc` — extract ProfileTag serialization, delete rest

**Changes:**

1. `workspace.h` — Add all fields from Profile (except workspace_id) plus keep existing type field:

```cpp
struct ProfileTag {
  std::string tag;
  std::string color;
  base::DictValue ToDict() const;
  static ProfileTag FromDict(const base::DictValue& dict);
};

struct Workspace {
  enum class Type { kNormal, kFingerprint };

  // Identity (from both)
  std::string id;
  std::string name;
  Type type = Type::kNormal;
  uint32_t color = 0xFF6366F1;           // from Profile
  std::string default_tab_title;          // from Profile
  std::string notes;                      // from Profile

  // Fingerprint (from Profile)
  int fingerprint_seed = 0;
  std::string user_agent;
  int screen_width = 1920;
  int screen_height = 1080;
  std::string gpu_vendor;
  std::string gpu_renderer;
  int hardware_concurrency = 0;
  std::string platform;
  std::string color_scheme;

  // Network (from Profile)
  std::string proxy;
  std::string timezone;
  std::string locale;
  bool geoip = false;

  // Behavior (from Profile)
  bool humanize = false;
  std::string human_preset;
  bool headless = false;
  bool clipboard_sync = true;
  std::vector<std::string> launch_args;
  bool auto_launch = false;

  // Tags (from Profile)
  std::vector<ProfileTag> tags;

  // Runtime (not persisted)
  std::string user_data_dir;
  std::string status;
  std::string cdp_url;

  // Metadata
  base::Time created_at;
  base::Time updated_at;

  // Serialization
  base::DictValue ToDict() const;
  static Workspace FromDict(const base::DictValue& dict);

  // Helpers
  static std::string GenerateId();
  static int GenerateSeed();
  static Workspace CreateBasic(const std::string& name, Type type);
  static const char* TypeToString(Type type);
  static Type StringToType(const std::string& str);
};
```

2. `workspace.cc` — `ToDict()` and `FromDict()` now include all the profile JSON keys (the same keys from profile.cc, but without `workspace_id` and without conditional empty-string skipping for cleaner schema). Add `GenerateSeed()`.

3. `profile.h` — Delete Profile struct and ProfileCollection. Keep ProfileTag (move to workspace.h or keep as standalone).
4. `profile.cc` — Delete Profile/ProfileCollection serialization. Keep ProfileTag code or move to workspace.cc.

---

### Task 2: Merge stores — WorkspaceStore gets Profile fields, ProfileStore deleted

**Files:**
- Modify: `chromium_src/purecloak/browser/profiles/workspace_store.h`
- Modify: `chromium_src/purecloak/browser/profiles/workspace_store.cc`
- Delete: `chromium_src/purecloak/browser/profiles/profile_store.h`
- Delete: `chromium_src/purecloak/browser/profiles/profile_store.cc`
- Modify: `chromium_src/purecloak/browser/profiles/store_provider.h`
- Modify: `chromium_src/purecloak/browser/profiles/store_provider.cc`

**Changes:**

1. `workspace_store.h` — Add `UpdateWorkspaceField` method (update individual fields by key), add full-field `UpdateWorkspace` overload. The observer stays the same.

2. `workspace_store.cc` — `WorkspaceStore::UpdateWorkspace` now takes a full `Workspace` object instead of just name. `CreateWorkspace` now sets all fields from the passed-in Workspace. The `SaveToDisk()` serializes using the merged Workspace::ToDict() which now includes all fields.

3. `store_provider.h` — Remove `GetProfileStore()`. Remove `profile_stores_` map.

4. Delete `profile_store.h` and `profile_store.cc`.

---

### Task 3: Update BUILD.gn

**Files:**
- Modify: `chromium_src/purecloak/browser/profiles/BUILD.gn`

**Changes:** Remove `profile_store.cc` from sources. Update dependencies. Remove any references to profile_store.

---

### Task 4: Update WebUI handler — merge profile CRUD into workspace CRUD

**Files:**
- Modify: `chromium_src/purecloak/browser/ui/webui/purecloak_handler.h`
- Modify: `chromium_src/purecloak/browser/ui/webui/purecloak_handler.cc`

**Changes:**

1. `purecloak_handler.h`:
   - Remove `ProfileStore* profile_store_` member
   - Remove: `HandleGetProfilesForWorkspace`, `HandleCreateProfile`, `HandleUpdateProfile`, `HandleDeleteProfile`
   - Keep all workspace handlers (they now carry the full config)

2. `purecloak_handler.cc`:
   - Remove `#include "purecloak/browser/profiles/profile_store.h"`
   - `HandleCreateWorkspace`: now accepts all config fields (name, type, proxy, UA, etc.)
   - `HandleUpdateWorkspace`: now accepts any field (not just name)
   - `HandleGetAllWorkspaces`: returns full config for each workspace (was returning only id/name/type)
   - `HandleLaunchWorkspace`: no longer looks up profiles — launches the workspace directly

---

### Task 5: Update side panel views — remove profile expansion, merge dialogs

**Files:**
- Modify: `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.h`
- Modify: `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_view.cc`
- Modify: `chromium_src/purecloak/browser/ui/views/side_panel/purecloak_side_panel_coordinator.cc`
- Modify: `chromium_src/purecloak/browser/ui/views/side_panel/workspace_management_dialogs.h`
- Modify: `chromium_src/purecloak/browser/ui/views/side_panel/workspace_management_dialogs.cc`

**Changes:**

1. `purecloak_side_panel_view.h`:
   - Remove `ProfileStore* profile_store_` member
   - Remove all profile-related methods: `OnCreateProfile`, `OnRenameProfile`, `OnDeleteProfile`, `RebuildProfileList`
   - Remove `std::map<std::string, bool> expanded_workspaces_`
   - Keep: `OnCreateWorkspaceClicked`, `OnRenameWorkspace`, `OnDeleteWorkspace`
   - Also remove workspace expand/collapse since each workspace IS the launchable unit

2. `purecloak_side_panel_view.cc`:
   - Remove `#include "purecloak/browser/profiles/profile_store.h"`
   - Constructor: remove `profile_store_` initialization and observer registration
   - `RebuildWorkspaceList`: each workspace card is now a single item (no expand/collapse, no profile sublist). Show: type badge, name, status, proxy indicator, launch button.
   - Remove: `expanded_workspaces_` logic, profile iteration, profile cards
   - Remove all profile handler methods

3. `purecloak_side_panel_coordinator.cc`: Remove ProfileStore creation

4. `workspace_management_dialogs.h/cc`:
   - Delete `CreateProfileDialog`, `RenameProfileDialog`, `DeleteProfileDialog`
   - `CreateWorkspaceDialog`: add all config fields (name, proxy, UA, screen, timezone, locale, platform, etc.)
   - `RenameWorkspaceDialog`: stays as name-only (or expand to full config)
   - `DeleteWorkspaceDialog`: stays (just removes the workspace)

---

### Task 6: Update launcher — workspace IS the profile

**Files:**
- Modify: `chromium_src/purecloak/browser/workspace_launcher.h`
- Modify: `chromium_src/purecloak/browser/workspace_launcher.cc`
- Modify: `chromium_src/purecloak/browser/workspace_profile_applier.h`
- Modify: `chromium_src/purecloak/browser/workspace_profile_applier.cc`

**Changes:**

1. `workspace_launcher.h`:
   - `RunningWorkspaceManager::Start()` changes from `(Workspace, vector<Profile>)` to `(Workspace)` — the workspace itself has all the fields
   - `RunningWorkspace` struct: remove `profiles` vector, add merged workspace reference

2. `workspace_launcher.cc`:
   - `Start()`: just takes a Workspace (which carries proxy/UA/args etc.)
   - `ContinueStart()`: `BuildCommandLine()` uses workspace fields directly instead of iterating profiles
   - Profile applier uses the single workspace's CDP injection

3. `workspace_profile_applier.cc`:
   - No longer iterates multiple profiles
   - `WorkspaceProfileApplier` takes the Workspace config directly
   - CDP injection applies the single workspace's settings

---

### Task 7: Update CDP/content — Profile→Workspace references

**Files:**
- Modify: `chromium_src/purecloak/content/anti_detection/anti_detection_engine.h`
- Modify: `chromium_src/purecloak/content/anti_detection/anti_detection_engine.cc` (if exists)
- Modify: `chromium_src/purecloak/content/profile_cdp_injector.h`
- Modify: `chromium_src/purecloak/content/profile_cdp_injector.cc`

**Changes:** Replace all `const Profile&` with `const Workspace&` in function signatures. Update internal references. No logic change — the fields are the same, just on a different struct.

---

### Task 8: Update WebUI HTML/JS

**Files:**
- Modify: `chromium_src/purecloak/resources/purecloak.html`

**Changes:**
- Remove profile CRUD UI sections
- Workspace creation dialog now shows all config fields (proxy, UA, screen, etc.)
- Workspace list items show full config summary, no expand/collapse for profiles
- Each workspace has a "Launch" button that launches the workspace directly

---

### Task 9: Update cloakbrowser-suite schema

**Files:**
- Modify: `cloakbrowser-suite/backend/database.py`
- Modify: `cloakbrowser-suite/backend/models.py`
- Modify: `cloakbrowser-suite/frontend/src/lib/api.ts`

**Changes:**

1. `database.py`:
   - Add `type TEXT NOT NULL DEFAULT 'normal'` column to profiles table
   - Add `color INTEGER NOT NULL DEFAULT 0xFF6366F1`
   - Add `default_tab_title TEXT DEFAULT ''`
   - Keep all existing fields (they already match)

2. `models.py`:
   - `ProfileCreate`: add `type: str = "normal"`, `color: int = 0xFF6366F1`, `default_tab_title: str = ""`
   - `ProfileResponse`: same additions
   - Keep `workspace_id` as optional for backward compat or remove

3. `api.ts`:
   - Add `type: string`, `color: number`, `defaultTabTitle: string`
   - Update creation data interface
