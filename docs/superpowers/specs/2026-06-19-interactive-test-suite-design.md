# Interactive UI Test Suite for PureCloak

## Goal
Comprehensive Playwright test suite for PureCloak browser UI, running in **headed** mode (visible) with screenshots at every step, covering all menus and user interactions.

## Architecture

```
tests/interactive/
├── runner.mjs              # Entry point: launch browser, orchestrate modules, collect results
├── modules/
│   ├── 01_main_menu.mjs    # Three-dot menu items (keyboard-driven)
│   ├── 02_tab_operations.mjs # Open/close/switch tabs
│   ├── 03_context_menus.mjs # Right-click page/link/image menus
│   ├── 04_bookmarks.mjs    # Bookmark creation, verification, deletion
│   └── 05_settings_browsing.mjs # Settings nav, search, history, downloads
└── screenshots/            # Output: timestamped PNG per test step
```

## Module Interface
Each module exports a single async function:
```js
export async function run({ browser, context, page, results, screenshotDir, BASE_URL })
```
Appends `{name, step, passed, error, screenshot?}` objects to `results`.

## Testing Techniques

| Technique | Usage |
|---|---|
| **Keyboard shortcuts** (`page.keyboard`) | Trigger browser chrome (menus, tabs) that Playwright can't DOM-select |
| **Direct navigation** (`page.goto`) | Verify WebUI pages (purecloak://settings, etc.) |
| **DOM assertions** (`page.locator`) | Check page content rendered correctly |
| **Screenshots** (`page.screenshot`) | Visual evidence for every step |
| **Command-line flags** | `--no-first-run`, `--disable-fre`, `--disable-search-engine-choice-screen` |

## Test Coverage

### Module 01: Main Menu
- Open three-dot menu (Alt+F) → verify visible
- Navigate to: New Tab, History, Downloads, Bookmarks, Settings, Help, Zoom, Print
- Each: trigger → wait → screenshot → verify target page

### Module 02: Tab Operations
- Ctrl+T new tab → verify tab count/page
- Ctrl+W close tab
- Ctrl+Tab / Ctrl+Shift+Tab switch tabs
- Ctrl+[1-9] jump to tab

### Module 03: Context Menus
- Right-click blank page → verify menu items (Back, Forward, Reload, Save As, etc.)
- Right-click text selection (Copy, Paste)
- Right-click image (Save Image As, Copy Image)

### Module 04: Bookmarks
- Ctrl+D bookmark current page → verify dialog
- Navigate to purecloak://bookmarks → verify bookmark present
- Delete bookmark

### Module 05: Settings & Browsing
- Navigate settings subpages (search, appearance, privacy)
- Navigate history page → verify entries
- Navigate downloads page
- Navigate version page

## Success Criteria
- All modules run to completion with `headless: false`
- Every step captures a screenshot
- Results summary shows pass/fail per step
- Zero crashes or hangs
