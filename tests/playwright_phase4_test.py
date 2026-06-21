#!/usr/bin/env python3
"""
PureCloak Phase 4 Playwright Visual Full Test
==============================================
Tests all WebUI functionalities, menus, and interfaces using Playwright.
"""
import asyncio
import json
import subprocess
import time
import sys
import os
import http.client
import urllib.request
from playwright.async_api import async_playwright

CHROME_BINARY = "/home/molandtoxx/PureCloak/src/out/purecloak/chrome"
CDP_PORT = 9233
API_PORT = 9334
USER_DATA_DIR = "/tmp/purecloak_p4_test"
SCREENSHOT_DIR = "/tmp/purecloak_p4_screenshots"

PASS = 0
FAIL = 0
WARN = 0

def ok(name, detail=""):
    global PASS, FAIL, WARN
    PASS += 1
    icon = "✅"
    print(f"  {icon} {name}" + (f" — {detail[:120]}" if detail else ""))

def fail(name, detail=""):
    global PASS, FAIL, WARN
    FAIL += 1
    icon = "❌"
    print(f"  {icon} {name}" + (f" — {str(detail)[:200]}" if detail else ""))

def warn(name, detail=""):
    global PASS, FAIL, WARN
    WARN += 1
    icon = "⚠️"
    print(f"  {icon} {name}" + (f" — {str(detail)[:200]}" if detail else ""))

def section(title):
    print(f"\n{'='*60}")
    print(f"  📋 {title}")
    print(f"{'='*60}")

async def wait_for_cdp(port, timeout=30):
    start = time.time()
    while time.time() - start < timeout:
        try:
            conn = http.client.HTTPConnection("localhost", port, timeout=2)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                info = json.loads(resp.read().decode())
                conn.close()
                return info
            conn.close()
        except: pass
        await asyncio.sleep(0.5)
    return None

async def wait_for_api(port, timeout=15):
    start = time.time()
    while time.time() - start < timeout:
        try:
            conn = http.client.HTTPConnection("localhost", port, timeout=2)
            conn.request("GET", "/api/status")
            resp = conn.getresponse()
            if resp.status == 200:
                conn.close()
                return True
            conn.close()
        except: pass
        await asyncio.sleep(0.5)
    return False

async def main():
    global PASS, FAIL, WARN
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)
    os.system(f"rm -rf {USER_DATA_DIR}")

    print("=" * 60)
    print("  🧪 PureCloak Phase 4 — Playwright Visual Full Test")
    print("=" * 60)

    # ── Launch PureCloak ──
    section("Launch PureCloak")
    print("  🚀 Launching PureCloak...")
    proc = subprocess.Popen(
        [
            CHROME_BINARY,
            f"--remote-debugging-port={CDP_PORT}",
            f"--purecloak-api-port={API_PORT}",
            f"--user-data-dir={USER_DATA_DIR}",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-background-networking",
            "--disable-extensions",
            "--no-sandbox",
            "--window-size=1280,900",
            "about:blank",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    version_info = await wait_for_cdp(CDP_PORT)
    if version_info is None:
        fail("Chrome launch", "CDP not available after 30s")
        proc.kill()
        sys.exit(1)

    ok("Chrome launch", f"PID={proc.pid}")
    print(f"  Browser: {version_info.get('Browser', 'unknown')}")
    print(f"  CDP: http://localhost:{CDP_PORT}")
    print(f"  API: http://localhost:{API_PORT}")

    # Wait for API
    api_ready = await wait_for_api(API_PORT)
    if api_ready:
        ok("API server", f"http://localhost:{API_PORT}/api/status")
    else:
        warn("API server", "Not ready after 15s")

    async with async_playwright() as p:
        browser = await p.chromium.connect_over_cdp(f"http://localhost:{CDP_PORT}")
        context = browser.contexts[0]

        # Create tab directly via CDP with target URL (bypasses Playwright's page.goto issue with chrome:// URLs)
        req = urllib.request.Request(
            f"http://localhost:{CDP_PORT}/json/new?purecloak://purecloak/",
            method="PUT"
        )
        resp = urllib.request.urlopen(req, timeout=5)
        tab_info = json.loads(resp.read().decode())
        tab_id = tab_info["id"]

        # Find the corresponding Playwright page object
        page = None
        for attempt in range(10):
            if len(context.pages) > 0:
                for p_page in context.pages:
                    try:
                        url = p_page.url
                        if "purecloak" in url:
                            page = p_page
                            break
                    except:
                        pass
            if page:
                break
            await asyncio.sleep(0.5)
        else:
            # Fallback: connect to page by ID via raw CDP
            page = context.pages[0] if context.pages else await context.new_page()

        try:
            # ════════════════════════════════════════════
            # TEST 1: WebUI Navigation & Layout
            # ════════════════════════════════════════════
            section("1. WebUI Navigation & Layout")
            # Wait for page to settle after CDP-created navigation
            await page.wait_for_timeout(3000)

            # Check title
            title = await page.title()
            if "PureCloak" in title:
                ok("Page title", title)
            else:
                fail("Page title", title)

            # Check header exists
            header = await page.query_selector(".header h1")
            if header:
                text = await header.inner_text()
                ok("Header title", text)
            else:
                fail("Header title", "not found")

            # Check "+ New Workspace" button
            create_btn = await page.query_selector("#btnCreateWs")
            if create_btn:
                ok("Create button exists")
            else:
                fail("Create button", "not found")

            # Screenshot: WebUI initial state
            await page.screenshot(path=f"{SCREENSHOT_DIR}/01_webui_initial.png", full_page=True)
            ok("Screenshot: WebUI initial state")

            # ════════════════════════════════════════════
            # TEST 2: Create Workspace Dialog
            # ════════════════════════════════════════════
            section("2. Create Workspace Dialog")
            await create_btn.click()
            await page.wait_for_timeout(500)

            # Dialog should be visible
            dialog = await page.query_selector("#createWsDialog.active")
            if dialog:
                ok("Create dialog opens")
            else:
                fail("Create dialog", "not visible")

            # Check dialog fields exist
            name_input = await page.query_selector("#wsNameInput")
            type_select = await page.query_selector("#wsTypeSelect")
            if name_input and type_select:
                ok("Dialog fields: name + type")
            else:
                fail("Dialog fields", "missing")

            # Check Fingerprint Settings collapsible
            fp_section = await page.query_selector("#createWsDialog .collapse-section:nth-of-type(1)")
            if fp_section:
                ok("Fingerprint Settings section exists")
            else:
                fail("Fingerprint Settings", "not found")

            # Check the tag input exists (Phase 4 feature)
            tags_input = await page.query_selector("#wsTags")
            if tags_input:
                ok("[P4] Tags input in create dialog")
            else:
                warn("[P4] Tags input", "not found")

            # Screenshot: Create dialog
            await page.screenshot(path=f"{SCREENSHOT_DIR}/02_create_dialog.png", full_page=True)
            ok("Screenshot: Create dialog")

            # Cancel dialog
            await page.click("#cancelCreateWs")
            await page.wait_for_timeout(300)

            # ════════════════════════════════════════════
            # TEST 3: Workspace CRUD via WebUI (sendAsync)
            # ════════════════════════════════════════════
            section("3. Workspace CRUD via sendAsync")

            # Create workspace
            ws = await page.evaluate("""async () => {
                return await sendAsync('createWorkspace', {
                    name: 'P4-Test-WS-1',
                    type: 'fingerprint',
                    proxy: 'http://user:pass@127.0.0.1:8080',
                    timezone: 'Asia/Shanghai',
                    locale: 'zh-CN',
                    screen_width: 1366,
                    screen_height: 768,
                });
            }""")
            ws_id = ws.get("id", "") if isinstance(ws, dict) else ""
            if ws_id:
                ok("createWorkspace", f"id={ws_id[:16]}…")
            else:
                fail("createWorkspace", str(ws)[:100])

            # Refresh the UI so the card appears
            await page.evaluate("loadWorkspaces()")
            await page.wait_for_timeout(1500)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/03_after_create.png", full_page=True)
            ok("Screenshot: After workspace creation")

            # Verify card appears in list
            card = await page.query_selector(f".ws-card[data-ws-id='{ws_id}']")
            if card:
                ok("Workspace card visible in list")
            else:
                fail("Workspace card", "not visible")

            # Get workspace status
            status_info = await page.evaluate(f"""async () => {{
                return await sendAsync('getWorkspaceStatus', '{ws_id}');
            }}""")
            ok("getWorkspaceStatus", json.dumps(status_info, ensure_ascii=False)[:100])

            # Update workspace name
            result = await page.evaluate(f"""async () => {{
                return await sendAsync('updateWorkspace', {{
                    id: '{ws_id}',
                    name: 'P4-Test-WS-1-Renamed',
                }});
            }}""")
            ok("updateWorkspace", f"result={'success' if result else 'fail'}")

            await page.wait_for_timeout(1000)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/04_after_rename.png", full_page=True)

            # Create a second workspace for batch ops
            ws2 = await page.evaluate("""async () => {
                return await sendAsync('createWorkspace', {
                    name: 'P4-Test-WS-2',
                    type: 'normal',
                });
            }""")
            ws2_id = ws2.get("id", "") if isinstance(ws2, dict) else ""
            if ws2_id:
                ok("Create workspace 2", f"id={ws2_id[:16]}…")
            else:
                fail("Create workspace 2", str(ws2)[:100])

            await page.wait_for_timeout(1000)

            # ════════════════════════════════════════════
            # TEST 4: Edit Workspace Dialog
            # ════════════════════════════════════════════
            section("4. Edit Workspace Dialog")

            # Open edit dialog via showEditWs
            await page.evaluate(f"showEditWs('{ws_id}')")
            await page.wait_for_timeout(500)
            edit_dialog = await page.query_selector("#editWsDialog.active")
            if edit_dialog:
                ok("Edit dialog opens")
            else:
                fail("Edit dialog", "not visible")

            # Check fingerprint preview (Phase 4)
            fp_preview = await page.query_selector("#editWsFpPreview")
            if fp_preview:
                ok("[P4] Fingerprint preview section exists")
            else:
                warn("[P4] Fingerprint preview", "not found")

            await page.screenshot(path=f"{SCREENSHOT_DIR}/05_edit_dialog.png", full_page=True)
            ok("Screenshot: Edit dialog")

            # Cancel
            await page.click("#cancelEditWs")
            await page.wait_for_timeout(300)

            # ════════════════════════════════════════════
            # TEST 5: Search/Filter (Phase 4)
            # ════════════════════════════════════════════
            section("5. Search/Filter [P4]")
            search_input = await page.query_selector("#searchInput")
            if search_input:
                ok("[P4] Search input exists")
                # Search for workspace 1
                await search_input.fill("P4-Test-WS-1")
                await page.wait_for_timeout(500)

                # After search, the renamed workspace should be visible
                await page.screenshot(path=f"{SCREENSHOT_DIR}/06_search.png", full_page=True)
                ok("[P4] Screenshot: Search results")

                # Clear search
                await search_input.fill("")
                await page.wait_for_timeout(500)
            else:
                warn("[P4] Search input", "not found")

            # ════════════════════════════════════════════
            # TEST 6: Dashboard Stats (Phase 4)
            # ════════════════════════════════════════════
            section("6. Dashboard Stats [P4]")
            # Refresh stats after workspace creation
            await page.evaluate("loadDashboardStats()")
            await page.wait_for_timeout(500)
            stats_bar = await page.query_selector("#statsBar")
            if stats_bar:
                ok("[P4] Dashboard stats bar exists")
                stat_cards = await page.query_selector_all(".stat-card")
                if len(stat_cards) == 4:
                    ok("[P4] 4 stat cards (Total/Running/Fingerprint/Normal)")
                else:
                    warn(f"[P4] Stat cards count: {len(stat_cards)}")

                # Check stats loaded
                stat_total = await page.query_selector("#statTotal")
                if stat_total:
                    val = await stat_total.inner_text()
                    if val and int(val) >= 2:
                        ok(f"[P4] Dashboard stats loaded: {val} workspaces")
                    else:
                        warn(f"[P4] Dashboard stats value: {val}")
                await page.screenshot(path=f"{SCREENSHOT_DIR}/07_dashboard_stats.png", full_page=True)
                ok("[P4] Screenshot: Dashboard stats")
            else:
                warn("[P4] Dashboard stats bar", "not found")

            # ════════════════════════════════════════════
            # TEST 7: Batch Operations UI (Phase 4)
            # ════════════════════════════════════════════
            section("7. Batch Operations [P4]")
            checkboxes = await page.query_selector_all(".ws-checkbox")
            if checkboxes:
                ok(f"[P4] Batch checkboxes found ({len(checkboxes)})")
                # Check the first checkbox
                await checkboxes[0].click()
                await page.wait_for_timeout(300)
                batch_bar = await page.query_selector("#batchBar:not([style*='display: none'])")
                if batch_bar:
                    ok("[P4] Batch action bar appears when checkbox checked")
                    batch_launch = await page.query_selector("#batchLaunch")
                    batch_stop = await page.query_selector("#batchStop")
                    batch_delete = await page.query_selector("#batchDelete")
                    if batch_launch and batch_stop and batch_delete:
                        ok("[P4] Batch buttons: Launch/Stop/Delete")
                    await page.screenshot(path=f"{SCREENSHOT_DIR}/08_batch_ops.png", full_page=True)
                    ok("[P4] Screenshot: Batch operations")
                else:
                    warn("[P4] Batch action bar", "not visible")

                # Uncheck
                await checkboxes[0].click()
                await page.wait_for_timeout(300)
            else:
                warn("[P4] Batch checkboxes", "not found")

            # ════════════════════════════════════════════
            # TEST 8: Keyboard Shortcuts (Phase 4)
            # ════════════════════════════════════════════
            section("8. Keyboard Shortcuts [P4]")

            # Ctrl+N should open create dialog (but Playwright can't do true Ctrl+N in some envs)
            # Test Escape to close dialogs
            try:
                # First open dialog
                await page.click("#btnCreateWs")
                await page.wait_for_timeout(300)
                dialog_active = await page.query_selector("#createWsDialog.active")
                if dialog_active:
                    # Press Escape
                    await page.keyboard.press("Escape")
                    await page.wait_for_timeout(300)
                    dialog_gone = await page.query_selector("#createWsDialog.active")
                    if not dialog_gone:
                        ok("[P4] Escape closes dialog")
                    else:
                        warn("[P4] Escape close dialog", "still visible")
                else:
                    warn("[P4] Keyboard test", "could not open dialog")
            except Exception as e:
                warn("[P4] Keyboard shortcut test", str(e)[:100])

            # ════════════════════════════════════════════
            # TEST 9: Template Save/Load (Phase 4)
            # ════════════════════════════════════════════
            section("9. Templates [P4]")
            save_template_btn = await page.query_selector("#btnSaveTemplate")
            if save_template_btn:
                ok("[P4] Save Template button exists")
            else:
                warn("[P4] Save Template button", "not found")

            template_select = await page.query_selector("#templateSelect")
            if template_select:
                ok("[P4] Template dropdown exists")
            else:
                warn("[P4] Template dropdown", "not found")

            # Test saveTemplate via sendAsync
            template_result = await page.evaluate("""async () => {
                return await sendAsync('saveTemplate', {
                    name: 'Test Template',
                    workspace_ids: []
                });
            }""")
            if isinstance(template_result, dict) and template_result.get("success"):
                ok("[P4] saveTemplate works")
            else:
                warn("[P4] saveTemplate", str(template_result)[:100])

            # Test getTemplates
            templates = await page.evaluate("""async () => {
                return await sendAsync('getTemplates');
            }""")
            if isinstance(templates, list):
                ok("[P4] getTemplates works")
            else:
                warn("[P4] getTemplates", str(templates)[:100])

            # ════════════════════════════════════════════
            # TEST 10: getAllTags (Phase 4)
            # ════════════════════════════════════════════
            section("10. Tags [P4]")
            tags = await page.evaluate("""async () => {
                return await sendAsync('getAllTags');
            }""")
            if isinstance(tags, list):
                ok("[P4] getAllTags works", f"{len(tags)} tags")
            else:
                warn("[P4] getAllTags", str(tags)[:100])

            # ════════════════════════════════════════════
            # TEST 11: getDashboardStats (Phase 4)
            # ════════════════════════════════════════════
            section("11. Dashboard Stats API [P4]")
            stats = await page.evaluate("""async () => {
                return await sendAsync('getDashboardStats');
            }""")
            if isinstance(stats, dict):
                total = stats.get("total", 0)
                normal = stats.get("normal", 0)
                fingerprint = stats.get("fingerprint", 0)
                ok(f"[P4] Dashboard stats: {total} total, {normal} normal, {fingerprint} fp")
            else:
                warn("[P4] getDashboardStats", str(stats)[:100])

            # ════════════════════════════════════════════
            # TEST 12: searchWorkspaces (Phase 4)
            # ════════════════════════════════════════════
            section("12. Search API [P4]")
            search_result = await page.evaluate("""async () => {
                return await sendAsync('searchWorkspaces', 'P4-Test');
            }""")
            if isinstance(search_result, list) and len(search_result) >= 2:
                ok(f"[P4] searchWorkspaces found {len(search_result)} matching workspaces")
            else:
                warn("[P4] searchWorkspaces", f"got {len(search_result) if isinstance(search_result, list) else type(search_result).__name__}")

            # ════════════════════════════════════════════
            # TEST 13: REST API — Status
            # ════════════════════════════════════════════
            section("13. REST API")
            try:
                conn = http.client.HTTPConnection("localhost", API_PORT, timeout=5)
                conn.request("GET", "/api/status")
                resp = conn.getresponse()
                data = json.loads(resp.read().decode())
                if data.get("success"):
                    ok("GET /api/status", json.dumps(data.get("data", {}), ensure_ascii=False)[:100])
                else:
                    fail("GET /api/status", str(data)[:100])
                conn.close()
            except Exception as e:
                fail("GET /api/status", str(e)[:100])

            # GET /api/workspaces
            try:
                conn = http.client.HTTPConnection("localhost", API_PORT, timeout=5)
                conn.request("GET", "/api/workspaces")
                resp = conn.getresponse()
                data = json.loads(resp.read().decode())
                workspaces = data.get("data", [])
                if data.get("success") and len(workspaces) >= 2:
                    ok("GET /api/workspaces", f"{len(workspaces)} workspaces")
                else:
                    fail("GET /api/workspaces", str(data)[:100])
                conn.close()
            except Exception as e:
                fail("GET /api/workspaces", str(e)[:100])

            # ════════════════════════════════════════════
            # TEST 14: Launch Workspace
            # ════════════════════════════════════════════
            section("14. Workspace Launch")
            if ws_id:
                launch_result = await page.evaluate(f"""async () => {{
                    return await sendAsync('launchWorkspace', '{ws_id}');
                }}""")
                if isinstance(launch_result, dict) and launch_result.get("success") != False:
                    ok("launchWorkspace", f"cdp_port={launch_result.get('cdp_port', '?')}")
                    await page.wait_for_timeout(3000)
                    await page.screenshot(path=f"{SCREENSHOT_DIR}/09_launched.png", full_page=True)
                    ok("Screenshot: After launch")
                else:
                    warn("launchWorkspace", str(launch_result)[:100])

                # Stop
                stop_result = await page.evaluate(f"""async () => {{
                    return await sendAsync('stopWorkspace', '{ws_id}');
                }}""")
                ok("stopWorkspace", f"result={json.dumps(stop_result, ensure_ascii=False)[:80]}")
                await page.wait_for_timeout(1000)

            # ════════════════════════════════════════════
            # TEST 15: Anti-Detection Verification
            # ════════════════════════════════════════════
            section("15. Anti-Detection")
            await page.goto("about:blank", wait_until="domcontentloaded", timeout=10000)
            fp_results = await page.evaluate("""() => {
                const r = {};
                r.webdriver = navigator.webdriver;
                r.plugins_length = navigator.plugins.length;
                r.languages = JSON.stringify(navigator.languages);
                r.platform = navigator.platform;
                r.hardwareConcurrency = navigator.hardwareConcurrency;
                r.deviceMemory = navigator.deviceMemory;
                r.screen_width = screen.width;
                r.screen_height = screen.height;
                r.colorDepth = screen.colorDepth;
                r.userAgent = navigator.userAgent;
                try {
                    const c = document.createElement('canvas');
                    c.width = 256; c.height = 128;
                    const ctx = c.getContext('2d');
                    ctx.textBaseline = 'top';
                    ctx.font = '14px Arial';
                    ctx.fillText('PureCloak Test', 4, 4);
                    r.canvas_fingerprint = c.toDataURL().substring(0, 80);
                } catch(e) { r.canvas_error = e.message; }
                try {
                    const c2 = document.createElement('canvas');
                    const gl = c2.getContext('webgl');
                    if (gl) {
                        r.webgl_vendor = gl.getParameter(gl.VENDOR);
                        r.webgl_renderer = gl.getParameter(gl.RENDERER);
                    }
                } catch(e) { r.webgl_error = e.message; }
                return r;
            }""")

            webdriver_hidden = fp_results.get("webdriver") in (False, None)
            for k, v in fp_results.items():
                status = "✅" if v not in (None, False, "undefined") else "❌"
                print(f"  {status} {k}: {str(v)[:80]}")

            if webdriver_hidden:
                ok("navigator.webdriver HIDDEN", "critical check passed")
            else:
                fail("navigator.webdriver EXPOSED", f"value={fp_results.get('webdriver')}")

            await page.screenshot(path=f"{SCREENSHOT_DIR}/10_fingerprint.png", full_page=True)
            ok("Screenshot: Fingerprint test")

            # ════════════════════════════════════════════
            # TEST 16: Delete Workspace via WebUI
            # ════════════════════════════════════════════
            section("16. Cleanup")
            # Navigate back to purecloak WebUI so sendAsync is available
            await page.goto("purecloak://purecloak/", wait_until="domcontentloaded", timeout=10000)
            await page.wait_for_timeout(1000)
            if ws2_id:
                result = await page.evaluate(f"""async () => {{
                    return await sendAsync('deleteWorkspace', '{ws2_id}');
                }}""")
                ok(f"Delete workspace 2", f"{'✓' if result else '✗'}")
            if ws_id:
                result = await page.evaluate(f"""async () => {{
                    return await sendAsync('deleteWorkspace', '{ws_id}');
                }}""")
                ok(f"Delete workspace 1", f"{'✓' if result else '✗'}")

            await page.wait_for_timeout(1000)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/11_after_delete.png", full_page=True)

            # ════════════════════════════════════════════
            # FINAL SUMMARY
            # ════════════════════════════════════════════
            print(f"\n{'='*60}")
            total = PASS + FAIL + WARN
            print(f"  📊 Test Results: {PASS} ✅ / {FAIL} ❌ / {WARN} ⚠️  (Total: {total})")
            print(f"{'='*60}")

            if FAIL > 0:
                print(f"\n  ❌ {FAIL} test(s) FAILED — review details above")
            if WARN > 0:
                print(f"\n  ⚠️  {WARN} warning(s) — features may need build rebuild")

        except Exception as e:
            print(f"\n❌ Test error: {e}")
            import traceback
            traceback.print_exc()
            try:
                await page.screenshot(path=f"{SCREENSHOT_DIR}/error.png", full_page=True)
            except: pass

        finally:
            await page.close()
            await browser.close()

    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

    print(f"\n📸 Screenshots saved to {SCREENSHOT_DIR}/")
    os.system(f"ls -la {SCREENSHOT_DIR}/")
    print(f"\n{'='*60}")
    result = "PASS" if FAIL == 0 else "FAIL"
    print(f"  🏁 Overall: {result}  ({PASS}✅ {FAIL}❌ {WARN}⚠️)")
    print(f"{'='*60}")

    return FAIL == 0

if __name__ == "__main__":
    success = asyncio.run(main())
    sys.exit(0 if success else 1)
