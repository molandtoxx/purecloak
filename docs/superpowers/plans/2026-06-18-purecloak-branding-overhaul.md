# PureCloak Branding Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all "chrome"/"chromium" references in PureCloak-owned code with "PureCloak", rename the binary from `chrome` to `purecloak`, and remove all Chromium default preset content (bookmarks, NTP, welcome pages).

**Architecture:** Three independent workstreams: (A) pure text replacement in ~12 files, (B) binary name from `chrome`→`purecloak` via GN build config, (C) suppress default bookmarks/NTP via `chromium_src/` overrides. All converge in verification phase D.

**Tech Stack:** C++ (Chromium), GN build system, `chromium_src/` overlay mechanism, Playwright (E2E).

---

## File Structure

### Files to Modify (Phase A — Text Replacement)

| # | File | What Change |
|---|------|-------------|
| A1 | `src/purecloak/browser/workspace_launcher.cc` | `"chrome"`→`"purecloak"` fallback path; comment |
| A2 | `src/purecloak/browser/profiles/workspace.h` | 3 comment lines |
| A3 | `src/purecloak/browser/ui/webui/purecloak_ui.h` | `chrome://purecloak/`→`//purecloak/` in 2 comments |
| A4 | `src/purecloak/browser/ui/webui/purecloak_handler.h` | Same as A3 |
| A5 | `src/purecloak/browser/purecloak_webui_registrar.h` | `ChromeBrowserMainParts`→`PureCloakBrowserMainParts` |
| A6 | `src/purecloak/tests/cdp_integration_test.cc` | `"out/purecloak/chrome"`→`"out/purecloak/purecloak"` |
| A7 | `src/purecloak/resources/purecloak.html` | Comment `Chromium WebUI`→`PureCloak WebUI` |
| A8 | `chromium_src/...` mirror of all above | Same changes (the two dirs are identical copies) |

### Files to Create/Modify (Phase B — Binary Name)

- Create: `chromium_src/purecloak/branding/post_build.sh` (post-build rename script)
- Modify: `src/purecloak/tests/cdp_integration_test.cc` (already in A6)
- Modify: `src/purecloak/browser/workspace_launcher.cc` (already in A1)

### Files to Create/Modify (Phase C — Default Content Cleanup)

- Create: `chromium_src/chrome/browser/resources/new_tab_page/purecloak_ntp.html` (blank NTP)
- Create or override: C++ source to suppress default bookmarks via `chromium_src/`

---

### Task 1: Replace text in all PureCloak source files

**Files to modify** (7 files in `src/purecloak/`):

**1a: `src/purecloak/browser/workspace_launcher.cc`**

- [ ] Change line 466 comment and line 467 fallback path:

```cpp
  // Fallback: assume "purecloak" is in PATH.
  return base::FilePath("purecloak");
```

(Note: the "Chrome subprocess" comment is in workspace.h, not this file — handled in 1b)

**1b: `src/purecloak/browser/profiles/workspace.h`**

- [ ] Change 3 comments:

Line 29: `// Workspace(1) → Profile(N), but after the merge, each Workspace carries`
  (check if "Chrome subprocess" appears → change to "PureCloak subprocess")

Line 69: `std::vector<std::string> launch_args;  // extra chromium flags`
  → `std::vector<std::string> launch_args;  // extra PureCloak flags`

Line 76: `std::string user_data_dir;  // Chromium profile directory`
  → `std::string user_data_dir;  // PureCloak profile directory`

**1c: `src/purecloak/browser/ui/webui/purecloak_ui.h`**

- [ ] Change comments (2 occurrences):

Line 16: `// WebUIConfig for chrome://purecloak/`
  → `// WebUIConfig for //purecloak/`

Line 23: `// WebUIController for chrome://purecloak/`
  → `// WebUIController for //purecloak/`

**1d: `src/purecloak/browser/ui/webui/purecloak_handler.h`**

- [ ] Change comment:

Line 21: `// WebUIMessageHandler for chrome://purecloak/.`
  → `// WebUIMessageHandler for //purecloak/.`

**1e: `src/purecloak/browser/purecloak_webui_registrar.h`**

- [ ] Change comment:

Line 11: `// Called from ChromeBrowserMainParts during startup, after`
  → `// Called from PureCloakBrowserMainParts during startup, after`

**1f: `src/purecloak/tests/cdp_integration_test.cc`**

- [ ] Change string:

Line 57: `return base::FilePath("out/purecloak/chrome");`
  → `return base::FilePath("out/purecloak/purecloak");`

**1g: `src/purecloak/resources/purecloak.html`**

- [ ] Change comment:

Line 610: `// --- Chromium WebUI callback shim (cr.webUIResponse bridge) ---`
  → `// --- PureCloak WebUI callback shim (cr.webUIResponse bridge) ---`

**1h: Mirror to `chromium_src/purecloak/`**

- [ ] Verify that `chromium_src/purecloak/` is an identical copy of `src/purecloak/`:

Run: `diff -r src/purecloak/ chromium_src/purecloak/` (expected: no output)

- [ ] Apply the same edits to `chromium_src/purecloak/` files if they differ.

If the two directories are identical, use a script to batch-copy changes:
```bash
for f in workspace_launcher.cc workspace.h purecloak_ui.h purecloak_handler.h purecloak_webui_registrar.h cdp_integration_test.cc purecloak.html; do
  cp "src/purecloak/$f" "chromium_src/purecloak/$f" 2>/dev/null || \
  find chromium_src/purecloak -name "$f" -exec cp "src/purecloak/$f" {} \;
done
```

**1i: Verify**

- [ ] Run: `grep -rn '\bchrome\b\|chromium' src/purecloak/ --include='*.cc' --include='*.h' --include='*.html' | grep -v '#include "chrome/' | grep -v 'chrome://'`

Expected: No remaining references (except allowed ones: `#include "chrome/"`, `chrome://`)

---

### Task 2: Rename binary from `chrome` to `purecloak`

**Context:** The Linux executable output name is set in `chrome/BUILD.gn` line 153 as `_chrome_output_name = "chrome"`. Modifying the upstream BUILD.gn via `chromium_src/` would require maintaining a complete fork, so we use a post-build approach instead.

- [ ] **2a: Create post-build rename script**

Create `chromium_src/purecloak/branding/post_build.sh`:

```bash
#!/bin/bash
# Post-build script: rename chrome binary to purecloak
# Called after autoninja completes

OUT_DIR="${1:-out/purecloak}"

if [ -f "$OUT_DIR/chrome" ] && [ ! -f "$OUT_DIR/purecloak" ]; then
  ln -f "$OUT_DIR/chrome" "$OUT_DIR/purecloak"
  echo "Created $OUT_DIR/purecloak (symlink to chrome)"
elif [ -f "$OUT_DIR/chrome" ] && [ -f "$OUT_DIR/purecloak" ]; then
  echo "$OUT_DIR/purecloak already exists"
else
  echo "No chrome binary found at $OUT_DIR/chrome"
  exit 1
fi
```

Make executable: `chmod +x chromium_src/purecloak/branding/post_build.sh`

- [ ] **2b: Create wrapper build script**

Create `chromium_src/purecloak/branding/build.sh`:

```bash
#!/bin/bash
set -e
cd "$(dirname "$0")/../../.."
autoninja -C out/purecloak chrome
./chromium_src/purecloak/branding/post_build.sh out/purecloak
```

Make executable: `chmod +x chromium_src/purecloak/branding/build.sh`

- [ ] **2c: Verify binary exists**

Run:
```bash
ls -la out/purecloak/purecloak
file out/purecloak/purecloak
```

Expected output: `purecloak: ELF 64-bit LSB executable, x86-64`
(It's a hardlink/symlink to the `chrome` binary)

- [ ] **2d: Update `CHROME_BIN` references**

In `cdp_integration_test.cc` line 54, the env var `CHROME_BIN` is checked. Add support for `PURECLOAK_BIN`:

```cpp
base::FilePath GetChromeBinaryPath() {
  const char* env = std::getenv("PURECLOAK_BIN");
  if (env && env[0])
    return base::FilePath(env);
  const char* env2 = std::getenv("CHROME_BIN");
  if (env2 && env2[0])
    return base::FilePath(env2);
  return base::FilePath("out/purecloak/purecloak");
}
```

---

### Task 3: Remove default bookmarks

**Context:** Chrome creates default bookmark items (like "Apps" shortcut) programmatically in C++. The code is in `chrome/browser/bookmarks/chrome_bookmark_client.cc` and related files.

- [ ] **3a: Find the bookmark creation code**

Run:
```bash
grep -rn 'apps.*bookmark\|AddURL.*apps\|bookmark_bar.*apps\|chrome.*apps' chromium_src/chrome/browser/bookmarks/ --include='*.cc' | head -10
```

Look for `ChromeBookmarkClient::Init` or similar that adds the "Apps" shortcut to the bookmark bar.

- [ ] **3b: Create `chromium_src/` override to suppress default bookmarks**

Search for the "Apps" bookmark shortcut creation:

```bash
grep -rn 'AddBookmark\|bookmark_bar.*apps\|apps.*bookmark_bar\|extension.*apps' chromium_src/chrome/browser/extensions/ --include='*.cc' | head -10
grep -rn 'managed_bookmarks\|InitialBookmark\|default_bookmark' chromium_src/chrome/browser/ --include='*.cc' | head -10
```

The most likely location is `chrome/browser/extensions/bookmark_app_helper.cc` or `chrome/browser/web_applications/`. 

**Approach (regardless of exact file):**
1. Copy the identified file to `chromium_src/` at the same relative path (e.g., `chromium_src/chrome/browser/extensions/bookmark_app_helper.cc`)
2. In the copied file, find the function that creates the "Apps" bookmark shortcut and no-op it (return early or remove the call)

Example pattern (for `bookmark_app_helper.cc`):
```cpp
// In the function that adds bookmark shortcuts:
// Comment out or guard with:
#if 0
  // PureCloak: suppress default bookmark shortcuts.
  original_code_that_adds_bookmarks();
#endif
```

Document the specific file and lines changed in a code comment.

- [ ] **3c: Verify bookmarks are gone**

Build and run:
```bash
./chromium_src/purecloak/branding/build.sh
out/purecloak/purecloak --no-first-run --user-data-dir=/tmp/purecloak_test_empty --disable-default-apps --no-default-browser-check about:blank
```

Open bookmarks bar — verify no default items appear.

---

### Task 4: Replace NTP with blank page

- [ ] **4a: Create blank NTP override**

Create `chromium_src/chrome/browser/resources/new_tab_page/purecloak_ntp.html`:

```html
<!doctype html>
<html>
<head><meta charset="utf-8"><title>PureCloak</title></head>
<body style="background:#fff;margin:0;min-height:100vh"></body>
</html>
```

- [ ] **4b: Set up NTP override in `chromium_src/`**

The NTP is registered in `chrome/browser/ui/webui/ntp/`. Create a chromium_src override at the same path to redirect to the blank page.

Check the current NTP registration:
```bash
grep -rn 'kChromeUINewTabURL\|NewTabUI\|new_tab_page' chromium_src/chrome/browser/ui/webui/ --include='*.cc' | head -10
```

Override the relevant file at `chromium_src/chrome/browser/ui/webui/ntp/` to use `purecloak_ntp.html` as the NTP source.

- [ ] **4c: Verify NTP is blank**

Launch browser, open new tab (Ctrl+T), verify: all-white page, no Google logo, no tiles, no search box.

---

### Task 5: Verification

- [ ] **5a: Compile check**

Rebuild:
```bash
cd chromium_src && autoninja -C out/purecloak purecloak_unittests
```

Expected: Build succeeds with no errors.

- [ ] **5b: Run unit tests**

```bash
out/purecloak/purecloak_unittests --gtest_filter='*PureCloak*:*CDP*:*Workspace*'
```

Expected: All tests pass.

- [ ] **5c: Visual E2E test**

```bash
cd /home/molandtoxx/PureCloak && python3 tests/visual_e2e_test.py
```

Expected: Chrome launches, `navigator.webdriver = false`, screenshots captured.

- [ ] **5d: Verify no "chrome" references remain**

```bash
grep -rn '\bchrome\b' src/purecloak/ --include='*.cc' --include='*.h' --include='*.html' | grep -v '#include "chrome/' | grep -v 'chrome://' | grep -v 'Copyright'
```

Expected: Empty (no remaining references).

---

## Dependencies

```
Task 1 (text replace) ──┐
                         ├── Task 5 (verification)
Task 2 (binary rename) ──┤
                         │
Task 3 (bookmarks) ──────┤
Task 4 (NTP) ────────────┘
```

Tasks 1-4 are independent and can run in parallel. Task 5 needs all to complete first.

## Verification Criteria

- No `"chrome"` or `"chromium"` string in PureCloak-owned source files (excluding `#include "chrome/"` and `chrome://` scheme)
- `out/purecloak/purecloak` binary exists
- No default bookmarks in launched workspace
- NTP is a blank white page
- All unit tests pass
- Visual E2E test passes (fingerprint injection, WebUI, workspace CRUD)
