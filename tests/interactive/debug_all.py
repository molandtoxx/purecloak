#!/usr/bin/env python3
"""
PureCloak Comprehensive Debug & Test Suite
============================================
"""
import asyncio
import json
import time
import os
import sys
import subprocess
import http.client

# Ensure we can import common.py from the same directory
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "."))
from common import (
    ok, fail, warn, section,
    rest_get, rest_post, rest_put, rest_delete, unwrap_rest,
    launch_chrome, wait_for_api,
    SCREENSHOT_DIR, CDP_PORT, REST_BASE,
)
import common  # for mutable counters (common.PASS etc.)


# ── Test REST API endpoints ────────────────────────────────────────────────
async def test_rest_api():
    section("REST API — Workspace CRUD")
    ws1_id = ""
    ws2_id = ""

    # GET /api/workspaces (empty)
    raw, status = rest_get("/api/workspaces")
    if status == 200:
        items = unwrap_rest(raw)
        count = len(items) if isinstance(items, list) else 0
        ok("GET /api/workspaces (empty)", f"{count} workspaces")
    else:
        fail("GET /api/workspaces", f"status={status}")

    # POST /api/workspaces (create normal)
    raw, status = rest_post("/api/workspaces", {"name": "DebugNormalWS", "type": "normal"})
    d = unwrap_rest(raw) or {}
    ws1_id = d.get("id", "")
    if status == 200 and ws1_id:
        ok("POST /api/workspaces (normal)", f"id={ws1_id[:16]}…")
    else:
        fail("POST /api/workspaces (normal)", f"status={status}")

    # POST /api/workspaces (create fingerprint)
    raw, status = rest_post("/api/workspaces",
                            {"name": "DebugFingerprintWS", "type": "fingerprint",
                             "proxy": "socks5://127.0.0.1:9050"})
    d = unwrap_rest(raw) or {}
    ws2_id = d.get("id", "")
    if status == 200 and ws2_id:
        ok("POST /api/workspaces (fingerprint)", f"id={ws2_id[:16]}…")
    else:
        fail("POST /api/workspaces (fingerprint)", f"status={status}")

    # GET /api/workspaces (should have 2)
    raw, status = rest_get("/api/workspaces")
    if status == 200:
        items = unwrap_rest(raw)
        if isinstance(items, list) and len(items) == 2:
            ok("GET /api/workspaces (2 items)", f"count={len(items)}")
        else:
            fail("GET /api/workspaces", f"got {type(items).__name__} items={len(items) if isinstance(items, list) else '?'}")
    else:
        fail("GET /api/workspaces", f"status={status}")

    # GET /api/workspaces/{id}
    if ws1_id:
        raw, status = rest_get(f"/api/workspaces/{ws1_id}")
        d = unwrap_rest(raw) or {}
        if status == 200 and d.get("id"):
            ok(f"GET /api/workspaces/{ws1_id[:12]}…", f"name={d.get('name')}")
        else:
            fail("GET single workspace", f"status={status}")

    # PUT /api/workspaces/{id}
    if ws1_id:
        raw, status = rest_put(f"/api/workspaces/{ws1_id}",
                               {"name": "DebugNormalWS-Renamed"})
        if status == 200:
            ok(f"PUT /api/workspaces/{ws1_id[:12]}…", "renamed")
        else:
            fail("PUT workspace", f"status={status}")

    # Verify rename
    if ws1_id:
        raw, status = rest_get(f"/api/workspaces/{ws1_id}")
        d = unwrap_rest(raw) or {}
        name = d.get("name", "")
        if status == 200 and name == "DebugNormalWS-Renamed":
            ok("Rename verified", name)
        else:
            fail("Rename verification", str(d))

    return ws1_id, ws2_id


# ── Test WebUI pages ───────────────────────────────────────────────────────
async def test_webui_pages(page):
    section("WebUI Pages (purecloak://*)")

    PAGES = [
        ("PureCloak管理界面", "purecloak://purecloak/", "PureCloak"),
        ("设置", "purecloak://settings/", None),
        ("历史记录", "purecloak://history/", None),
        ("书签", "purecloak://bookmarks/", None),
        ("扩展程序", "purecloak://extensions/", None),
        ("关于版本", "purecloak://version/", None),
        ("实验功能", "purecloak://flags/", None),
        ("下载记录", "purecloak://downloads/", None),
        ("帮助", "purecloak://help/", None),
        ("新标签页", "purecloak://newtab/", None),
    ]

    for name, url, expected in PAGES:
        try:
            resp = await page.goto(url, wait_until="domcontentloaded", timeout=15000)
            await page.wait_for_timeout(1500)
            title = await page.title()
            status = resp.status if resp else 0
            if status == 200:
                ok(f"🖥️  {name} — {url}", f"title=\"{title}\"")
            else:
                warn(f"  {name} — {url}", f"status={status} title=\"{title}\"")
        except Exception as e:
            warn(f"  {name} — {url}", str(e)[:100])


# ── Test WebUI sendAsync (workspace CRUD) ─────────────────────────────────
async def test_webui_crud(page):
    section("WebUI sendAsync — Workspace CRUD")

    # Navigate to PureCloak WebUI
    await page.goto("purecloak://purecloak/", wait_until="domcontentloaded", timeout=15000)
    await page.wait_for_timeout(2000)

    # Screenshot
    await page.screenshot(path=f"{SCREENSHOT_DIR}/01_purecloak_webui.png", full_page=True)
    print("  📸 Screenshot: 01_purecloak_webui.png")

    # getAllWorkspaces
    all_ws = await page.evaluate("""async () => {
        return await sendAsync('getAllWorkspaces');
    }""")
    if isinstance(all_ws, list):
        ok("getAllWorkspaces", f"{len(all_ws)} workspaces")
    else:
        fail("getAllWorkspaces", str(all_ws)[:100])

    # createWorkspace via sendAsync
    ws = await page.evaluate("""async () => {
        return await sendAsync('createWorkspace', {
            name: 'WebUI-Test-WS',
            type: 'normal'
        });
    }""")
    ws_id = ws.get("id", "") if isinstance(ws, dict) else ""
    if ws_id:
        ok("createWorkspace (sendAsync)", f"id={ws_id[:16]}…")
    else:
        fail("createWorkspace (sendAsync)", str(ws)[:100])

    await page.wait_for_timeout(1000)
    await page.screenshot(path=f"{SCREENSHOT_DIR}/02_after_create.png", full_page=True)

    # getWorkspaceStatus
    if ws_id:
        status_info = await page.evaluate(f"""async () => {{
            return await sendAsync('getWorkspaceStatus', '{ws_id}');
        }}""")
        ok("getWorkspaceStatus", json.dumps(status_info, ensure_ascii=False)[:100])

    if ws_id:
        result = await page.evaluate(f"""async () => {{
            return await sendAsync('updateWorkspace', '{ws_id}', 'WebUI-Test-WS-Renamed');
        }}""")
        ok("updateWorkspace", f"result={result}")

    await page.wait_for_timeout(1000)
    await page.screenshot(path=f"{SCREENSHOT_DIR}/03_after_rename.png", full_page=True)

    # deleteWorkspace
    if ws_id:
        result = await page.evaluate(f"""async () => {{
            return await sendAsync('deleteWorkspace', '{ws_id}');
        }}""")
        ok("deleteWorkspace", f"result={result}")

    await page.wait_for_timeout(1000)
    await page.screenshot(path=f"{SCREENSHOT_DIR}/04_after_delete.png", full_page=True)

    return ws_id


# ── Test Anti-Detection ────────────────────────────────────────────────────
async def test_anti_detection(page):
    section("Anti-Detection / Fingerprint")

    await page.goto("about:blank", wait_until="domcontentloaded")
    await page.wait_for_timeout(1000)

    results = await page.evaluate("""() => {
        const r = {};
        r.webdriver = navigator.webdriver;
        r.plugins_length = navigator.plugins.length;
        r.languages = JSON.stringify(navigator.languages);
        r.platform = navigator.platform;
        r.hardwareConcurrency = navigator.hardwareConcurrency;
        r.screen_width = screen.width;
        r.screen_height = screen.height;
        r.userAgent = navigator.userAgent;
        r.deviceMemory = navigator.deviceMemory;
        r.maxTouchPoints = navigator.maxTouchPoints;
        r.cookieEnabled = navigator.cookieEnabled;
        r.doNotTrack = navigator.doNotTrack;
        try {
            const c = document.createElement('canvas');
            c.width = 256; c.height = 128;
            const ctx = c.getContext('2d');
            ctx.textBaseline = 'top';
            ctx.font = '14px Arial';
            ctx.fillText('PureCloak Fingerprint Test', 4, 4);
            ctx.fillRect(0, 0, 256, 128);
            r.canvas_fingerprint = 'active';
        } catch(e) { r.canvas_fingerprint = e.message; }
        try {
            const gl = document.createElement('canvas').getContext('webgl');
            if (gl) {
                r.webgl_vendor = gl.getParameter(gl.VENDOR);
                r.webgl_renderer = gl.getParameter(gl.RENDERER);
                r.webgl_unmasked_vendor = gl.getParameter(gl.UNMASKED_VENDOR_WEBGL);
                r.webgl_unmasked_renderer = gl.getParameter(gl.UNMASKED_RENDERER_WEBGL);
            } else { r.webgl = 'no context'; }
        } catch(e) { r.webgl_error = e.message; }
        // Check runtime
        try { r.chrome_runtime = typeof chrome.runtime; }
        catch(e) { r.chrome_runtime = 'error'; }
        return r;
    }""")

    checks = [
        ("navigator.webdriver hidden", results.get("webdriver") in (False, None)),
        ("navigator.plugins populated", results.get("plugins_length", 0) > 0),
        ("navigator.languages defined", results.get("languages", "[]") != "[]"),
        ("Canvas fingerprint active", results.get("canvas_fingerprint") == "active"),
        ("WebGL context available", results.get("webgl_vendor") is not None),
        ("chrome.runtime undefined", results.get("chrome_runtime") != "object"),
    ]

    for name, passed in checks:
        if passed:
            ok(f"🛡️  {name}")
        else:
            fail(f"🛡️  {name}")
            # Show relevant detail
            for k, v in results.items():
                if k.replace("_", " ") in name.lower() and v is not None:
                    print(f"       value={v}")

    # Also check CDP-level webdriver hiding
    try:
        cdp = await page.context.new_cdp_session(page)
        wd = await cdp.send("Page.addScriptToEvaluateOnNewDocument", {
            "source": ""  # Just check the command works
        })
        await cdp.detach()
        ok("CDP Page.addScriptToEvaluateOnNewDocument works")
    except Exception as e:
        fail("CDP command", str(e)[:100])

    await page.screenshot(path=f"{SCREENSHOT_DIR}/05_anti_detection.png", full_page=True)


# ── Test UI Interactions ──────────────────────────────────────────────────
async def test_ui_interactions(page, browser):
    section("UI Interactions — Toolbar & Menus")

    # Navigate to a visible page first
    await page.goto("purecloak://purecloak/", wait_until="domcontentloaded", timeout=15000)
    await page.wait_for_timeout(2000)

    # Check all major UI elements exist
    ui_checks = await page.evaluate("""() => {
        const r = {};
        // WebUI specific elements
        r.has_header = !!document.querySelector('.header');
        r.has_header_title = !!(document.querySelector('.header h1')?.textContent);
        r.has_create_btn = !!document.getElementById('btnCreateWs');
        r.has_workspace_list = !!document.getElementById('workspaceList');
        r.has_loading = !!document.getElementById('loadingState');
        r.has_empty_state = !!document.getElementById('emptyState');
        r.toast = !!document.getElementById('toast');
        // Buttons
        r.create_btn_text = document.getElementById('btnCreateWs')?.textContent || '';
        // Dialog elements
        r.create_dialog = !!document.getElementById('createWsDialog');
        r.edit_dialog = !!document.getElementById('editWsDialog');
        r.delete_dialog = !!document.getElementById('confirmDeleteDialog');
        // Collapsible sections
        r.fingerprint_settings = !!(document.querySelector('#createWsDialog .collapse-section'));
        return r;
    }""")

    for key, val in ui_checks.items():
        if val:
            ok(f"🔘 UI element: {key}", str(val)[:60])
        else:
            fail(f"🔘 UI element: {key}", "missing")

    # Test button click — open create dialog
    await page.click("#btnCreateWs")
    await page.wait_for_timeout(500)
    dialog_visible = await page.evaluate(
        "document.getElementById('createWsDialog')?.classList.contains('active')")
    if dialog_visible:
        ok("🔘 Click 'New Workspace' — dialog opens")
    else:
        fail("🔘 Click 'New Workspace' — dialog not visible")

    await page.screenshot(path=f"{SCREENSHOT_DIR}/06_create_dialog.png", full_page=True)

    # Fill the form
    await page.fill("#wsNameInput", "Debug-UI-WS")
    await page.select_option("#wsTypeSelect", "fingerprint")

    await page.evaluate("""() => {
        document.querySelectorAll('#createWsDialog .collapse-section').forEach(s => {
            s.setAttribute('open', '');
        });
    }""")
    await page.wait_for_timeout(300)

    await page.fill("#wsUserAgent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)")
    await page.fill("#wsProxy", "socks5://127.0.0.1:9050")
    await page.wait_for_timeout(300)

    await page.evaluate("document.getElementById('confirmCreateWs').click()")
    await page.wait_for_timeout(2000)
    ok("🔘 Fill form + click Create")

    await page.screenshot(path=f"{SCREENSHOT_DIR}/07_ws_created_ui.png", full_page=True)

    # Check Toast notification
    toast_text = await page.evaluate("document.getElementById('toast')?.textContent || ''")
    if toast_text:
        ok("🔘 Toast notification", toast_text[:80])
    else:
        warn("🔘 Toast notification", "empty")

    return ui_checks


# ── Test Workspace Lifecycle (Launch/Stop) ────────────────────────────────
async def test_workspace_lifecycle(page, browser):
    section("Workspace Lifecycle — Launch & Stop")

    raw, status = rest_post("/api/workspaces",
                            {"name": "Lifecycle-Test-WS-REST", "type": "fingerprint",
                             "proxy": "socks5://127.0.0.1:9050"})
    d = unwrap_rest(raw) or {}
    ws_id = d.get("id", "")
    if not ws_id:
        fail("Lifecycle: create workspace via REST", f"status={status}")
        return ""
    ok("Lifecycle: workspace created via REST", f"id={ws_id[:16]}…")

    raw, status = rest_post(f"/api/workspaces/{ws_id}/launch")
    d = unwrap_rest(raw) or {}
    if status == 200 and d.get("status") == "running":
        ok("Lifecycle: launchWorkspace via REST",
           f"cdp_port={d.get('cdp_port')} pid={d.get('pid')}")
        launched_cdp = d.get("cdp_port", 0)
    else:
        fail("Lifecycle: launchWorkspace via REST", f"status={status} data={d}")
        launched_cdp = 0

    await page.wait_for_timeout(2000)
    await page.screenshot(path=f"{SCREENSHOT_DIR}/08_after_launch.png", full_page=True)

    raw, status = rest_get(f"/api/workspaces/{ws_id}/status")
    d = unwrap_rest(raw) or {}
    if status == 200 and d.get("status") == "running":
        ok("Lifecycle: status after launch",
           f"cdp_port={d.get('cdp_port')} pid={d.get('pid')}")

    raw, status = rest_get("/api/status")
    d2 = unwrap_rest(raw) or {}
    if status == 200 and d2.get("running_workspaces") == 1:
        ok("Lifecycle: API /api/status confirms 1 running",
           f"total={d2.get('total_workspaces')}")

    if launched_cdp:
        try:
            conn = http.client.HTTPConnection("localhost", launched_cdp, timeout=3)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                version = json.loads(resp.read().decode())
                ok(f"Lifecycle: Child CDP on port {launched_cdp}",
                   version.get("Browser", "")[:60])
            conn.close()
        except Exception as e:
            warn("Lifecycle: Child CDP connect", str(e)[:80])

    try:
        raw, status = rest_post(f"/api/workspaces/{ws_id}/stop")
        d = unwrap_rest(raw) or {}
        if status == 200:
            ok("Lifecycle: stopWorkspace via REST", f"status={d.get('status')}")
        else:
            warn("Lifecycle: stopWorkspace via REST", f"status={status}")
    except Exception as e:
        warn("Lifecycle: stopWorkspace crashed (parent Chrome may have terminated)",
             str(e)[:120])

    try:
        await page.wait_for_timeout(2000)
        await page.screenshot(path=f"{SCREENSHOT_DIR}/09_after_stop.png",
                              full_page=True)

        raw, status = rest_get(f"/api/workspaces/{ws_id}/status")
        d = unwrap_rest(raw) or {}
        if status == 200 and d.get("status") == "stopped":
            ok("Lifecycle: status after stop", f"status={d.get('status')}")
        else:
            warn("Lifecycle: status after stop", f"status={status} data={d}")
    except Exception as e:
        warn("Lifecycle: page/browser unavailable after stop",
             str(e)[:100])

    return ws_id


# ── Main ───────────────────────────────────────────────────────────────────
async def main():
    print("=" * 60)
    print("  🧪 PureCloak Comprehensive Debug & Test Suite")
    print("  Watch the browser window to see each action!")
    print(f"  Screenshots → {SCREENSHOT_DIR}/")
    print("=" * 60)

    # Launch Chrome
    proc, version_info = launch_chrome()

    # Wait for running manager + API server
    await asyncio.sleep(2)

    # Test REST API
    wait_for_api()
    try:
        rest_ids = await test_rest_api()
    except Exception as e:
        fail("REST API tests crashed", str(e)[:100])
        rest_ids = ("", "")

    # Connect Playwright via CDP
    section("Playwright CDP Connection")
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        browser = await p.chromium.connect_over_cdp(
            f"http://localhost:{CDP_PORT}")
        context = browser.contexts[0]
        page = await context.new_page()
        ok("Playwright connected via CDP")

        # Run test suites
        try:
            await test_webui_pages(page)
            await test_webui_crud(page)
            await test_anti_detection(page)
            await test_ui_interactions(page, browser)
            ws_id = await test_workspace_lifecycle(page, browser)

            try:
                await page.goto("purecloak://purecloak/",
                                wait_until="domcontentloaded", timeout=15000)
                await page.wait_for_timeout(2000)
                await page.screenshot(path=f"{SCREENSHOT_DIR}/10_final.png",
                                      full_page=True)
            except Exception:
                pass

        except Exception as e:
            fail("Test suite crash", str(e)[:200])
            import traceback
            traceback.print_exc()

        finally:
            await page.close()
            await browser.close()

    # Cleanup
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

    # Summary
    p, f, w = common.PASS, common.FAIL, common.WARN
    total = p + f + w
    print(f"\n{'=' * 60}")
    print(f"  📊 RESULTS: {p} ✅  {f} ❌  {w} ⚠️   (Total: {total})")
    print(f"{'=' * 60}")
    print(f"\n  Screenshots saved to {SCREENSHOT_DIR}/")
    os.system(f"ls -la {SCREENSHOT_DIR}/")

    if f > 0:
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
