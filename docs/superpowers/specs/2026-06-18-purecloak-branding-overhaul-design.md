# PureCloak Branding Overhaul & Default Content Cleanup

**Date:** 2026-06-18
**Status:** Draft
**Approach:** A (Pragmatic Deep Transformation)

## 1. Goals

1. **Binary name:** Rename the built executable from `chrome` to `purecloak`.
2. **Code references:** In PureCloak-owned source files, replace all non-structural references to "chrome" / "chromium" with "PureCloak" (comments, string literals, variable descriptions).
3. **Preset content:** Remove all inherited Chromium default bookmarks, NTP preset content, and welcome/onboarding pages.
4. **Non-goals (per Approach A):**
   - Do NOT rename `#include "chrome/..."` paths (they reference upstream directory structure).
   - Do NOT change the `chrome://` internal URL scheme (users never see it; it's internal infrastructure).

## 2. Scope

### 2.1 Files to modify

**PureCloak source files (`src/purecloak/` and `chromium_src/purecloak/`):**

| File | Change | Type |
|------|--------|------|
| `browser/workspace_launcher.cc` | `"chrome"` → `"purecloak"` fallback path; comment update | String + comment |
| `browser/profiles/workspace.h` | `// extra chromium flags` → `// extra PureCloak flags` | Comment |
| `browser/profiles/workspace.h` | `// Chromium profile directory` → `// PureCloak profile directory` | Comment |
| `browser/profiles/workspace.h` | `// one Chrome subprocess` → `// one PureCloak subprocess` | Comment |
| `browser/ui/webui/purecloak_ui.h` | `chrome://purecloak/` → `//purecloak/` in comments | Comment |
| `browser/ui/webui/purecloak_handler.h` | Same | Comment |
| `browser/purecloak_webui_registrar.h` | `// Called from ChromeBrowserMainParts` → `PureCloakBrowserMainParts` | Comment |
| `tests/cdp_integration_test.cc` | `"out/purecloak/chrome"` → `"out/purecloak/purecloak"` | String |
| `resources/purecloak.html` | `// --- Chromium WebUI callback shim ---` | Comment |
| `browser/profiles/workspace.h` | `// each Workspace maps to one Chrome subprocess` → `// ... one PureCloak subprocess` | Comment |

### 2.2 Binary output name change

- The `chrome` executable is declared in `chrome/BUILD.gn` with `output_name = "chrome"`.
- Mechanism TBD in implementation: options include (a) `chromium_src/chrome/BUILD.gn` override for the executable target, (b) GN branding config modification, or (c) post-build symlink/copy.
- PureCloak already has `purecloak.gni` and `is_purecloak` GN arg — use these to conditionally set the output name.

### 2.3 Default content removal

**Default bookmarks:**
- Override `chrome/browser/resources/default_bookmarks/` via `chromium_src/` with empty bookmark data.

**New Tab Page (NTP):**
- Override NTP resources with a minimal blank page via `chromium_src/chrome/browser/resources/new_tab_page/`.

**Welcome / First Run:**
- Already handled by `--no-first-run` flag. Confirm no additional change needed.

## 3. Non-Changes (Justification)

### 3.1 `#include "chrome/..."` paths

These reference the physical `chrome/` directory in Chromium's source tree. Renaming the directory would require maintaining a full fork. Since these are implementation details not visible to users, they remain as-is.

### 3.2 `chrome://purecloak/` URL scheme

The `chrome://` scheme is registered in `content/common/url_schemes.cc` and hardcoded in hundreds of locations across `content/` and `chrome/`. Users never type or see this URL — it's a WebUI routing mechanism. The WebUI HTML title already says "PureCloak". Keeping `chrome://purecloak/` has zero user-facing impact.

## 4. Execution Plan

### Phase A — Text Replacement (no risk, parallelizable)
- A1: Replace all 10-12 comment/string occurrences in `.cc`/`.h` files
- A2: Update `purecloak.html` comment
- A3: Update binary path in test file

### Phase B — Binary Name Change
- B1: Research Chromium's `output_name` override mechanism
- B2: Add `chromium_src/` GN override to produce `purecloak` binary
- B3: Update test references to new binary name

### Phase C — Default Content Cleanup
- C1: Create empty default bookmarks override
- C2: Create blank NTP override
- C3: Verify first-run is disabled

### Phase D — Verification
- D1: Rebuild affected targets
- D2: Run `purecloak_unittests`
- D3: Run visual E2E test

## 5. Dependencies

- Phase B depends on Phase A (binary name changes in code reference the new name)
- Phase C is independent of A and B (can run in parallel)
- Phase D depends on all previous phases

## 6. Verification Criteria

- `purecloak` binary exists in `out/purecloak/` (renamed from `chrome`)
- No `"chrome"` or `"chromium"` string appears in PureCloak-owned source comments/strings
- `chrome://purecloak/` WebUI still works
- No default bookmarks appear in launched workspace
- NTP shows blank content (no Google branding, no preset tiles)
- All unit tests pass
- Visual E2E test passes
