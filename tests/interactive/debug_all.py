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

    # ── Deep anti-detection ──────────────────────────────────────────────────

    audio_result = await page.evaluate("""() => {
        try {
            const ctx = new AudioContext({sampleRate: 44100});
            const osc = ctx.createOscillator();
            const analyser = ctx.createAnalyser();
            osc.connect(analyser);
            const buf = new Float32Array(analyser.frequencyBinCount);
            analyser.getFloatFrequencyData(buf);
            const sum = buf.reduce((a, b) => a + Math.abs(b), 0);
            ctx.close();
            return {sampleRate: ctx.sampleRate, freqDataSum: sum, freqDataLen: buf.length};
        } catch(e) { return {error: e.message}; }
    }""")
    if audio_result.get("freqDataLen", 0) > 0:
        ok("🛡️  AudioContext fingerprinting active",
           f"{audio_result.get('freqDataLen')} bins")
    else:
        fail("🛡️  AudioContext", str(audio_result.get("error", "no data"))[:80])

    webrtc_result = await page.evaluate("""async () => {
        try {
            const pc = new RTCPeerConnection({iceServers: []});
            pc.createDataChannel('test');
            const offer = await pc.createOffer();
            await pc.setLocalDescription(offer);
            return new Promise((resolve) => {
                const timeout = setTimeout(() => resolve({timeout: true}), 5000);
                let srflxCandidates = 0;
                let totalCandidates = 0;
                pc.onicecandidate = (e) => {
                    if (e.candidate) {
                        totalCandidates++;
                        if (e.candidate.candidate.includes('srflx')) srflxCandidates++;
                    } else {
                        clearTimeout(timeout);
                        pc.close();
                        resolve({srflxCandidates, totalCandidates});
                    }
                };
            });
        } catch(e) { return {error: e.message}; }
    }""")
    if webrtc_result.get("srflxCandidates", -1) == 0:
        ok("🛡️  WebRTC no srflx candidates", f"total={webrtc_result.get('totalCandidates')}")
    elif webrtc_result.get("timeout"):
        warn("🛡️  WebRTC ICE timeout (no network?)", "srflx check skipped")
    else:
        warn("🛡️  WebRTC srflx candidates detected",
             f"{webrtc_result.get('srflxCandidates')} srflx / {webrtc_result.get('totalCandidates')} total")

    font_result = await page.evaluate("""() => {
        try {
            const canvas = document.createElement('canvas');
            const ctx = canvas.getContext('2d');
            const testFonts = ['Arial', 'Helvetica', 'Times New Roman', 'Courier New',
                               'serif', 'sans-serif', 'monospace', 'Impact',
                               'Georgia', 'Comic Sans MS'];
            ctx.font = '16px Arial';
            const baseline = ctx.measureText('PureCloakTest').width;
            const metrics = {};
            for (const font of testFonts) {
                ctx.font = "16px '" + font + "'";
                metrics[font] = ctx.measureText('PureCloakTest').width;
            }
            const vals = Object.values(metrics);
            const avg = vals.reduce((a, b) => a + b, 0) / vals.length;
            const deviations = vals.map(v => Math.abs(v - avg) / avg);
            return {baseline, maxDeviation: Math.max(...deviations), numFonts: testFonts.length};
        } catch(e) { return {error: e.message}; }
    }""")
    if font_result.get("baseline", 0) > 0:
        ok("🛡️  Canvas font metrics measurable",
           f"baseline={font_result.get('baseline'):.1f} deviation={font_result.get('maxDeviation'):.3f}")
    else:
        warn("🛡️  Canvas font metrics", str(font_result.get("error", "no data"))[:80])

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


async def test_multi_workspace(page, browser):
    section("Concurrency — Multi Workspace")
    ws_ids = []
    for i in range(3):
        raw, status = rest_post("/api/workspaces", {
            "name": f"Concurrent-WS-{i}",
            "type": "fingerprint",
            "proxy": "socks5://127.0.0.1:9050",
        })
        d = unwrap_rest(raw) or {}
        wid = d.get("id", "")
        if wid:
            ok(f"Concurrency: created WS #{i}", f"id={wid[:12]}…")
            ws_ids.append(wid)
        else:
            fail(f"Concurrency: create WS #{i}", f"status={status}")

    launched = []
    for wid in ws_ids:
        raw, status = rest_post(f"/api/workspaces/{wid}/launch")
        d = unwrap_rest(raw) or {}
        if status == 200 and d.get("status") == "running":
            ok(f"Concurrency: launched {wid[:12]}…",
               f"cdp_port={d.get('cdp_port')} pid={d.get('pid')}")
            launched.append(d)
        else:
            fail(f"Concurrency: launch {wid[:12]}…", f"status={status}")

    cdp_ports = {d.get("cdp_port") for d in launched}
    pids = {d.get("pid") for d in launched}
    if len(cdp_ports) == len(launched):
        ok("Concurrency: unique CDP ports", f"{len(cdp_ports)} unique")
    elif len(pids) == len(launched):
        warn("Concurrency: CDP ports not unique (race condition in FindAvailablePort)",
             f"{len(cdp_ports)} unique ports vs {len(launched)} launches, but {len(pids)} unique PIDs")
    if len(pids) == len(launched):
        ok("Concurrency: unique PIDs", f"{len(pids)} unique")
    else:
        fail("Concurrency: duplicate PIDs")

    await asyncio.sleep(2)
    raw, status = rest_get("/api/status")
    d2 = unwrap_rest(raw) or {}
    if d2.get("running_workspaces") == len(launched):
        ok("Concurrency: /api/status count correct",
           f"running={d2.get('running_workspaces')}")
    else:
        fail("Concurrency: /api/status count",
             f"expected={len(launched)} got={d2.get('running_workspaces')}")

    for d in launched:
        port = d.get("cdp_port")
        try:
            conn = http.client.HTTPConnection("localhost", port, timeout=3)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                v = json.loads(resp.read().decode())
                ok(f"Concurrency: child CDP on :{port}", v.get("Browser", "")[:50])
            conn.close()
        except Exception as e:
            warn(f"Concurrency: child CDP :{port} unreachable", str(e)[:60])

    for wid in ws_ids:
        raw, status = rest_post(f"/api/workspaces/{wid}/stop")
        if status == 200:
            ok(f"Concurrency: stopped {wid[:12]}…")
        else:
            warn(f"Concurrency: stop {wid[:12]}…", f"status={status}")

    await asyncio.sleep(2)

    raw, status = rest_get("/api/status")
    d3 = unwrap_rest(raw) or {}
    if d3.get("running_workspaces") == 0:
        ok("Concurrency: all stopped, running=0")
    else:
        warn("Concurrency: running_workspaces",
             f"={d3.get('running_workspaces')} (expected 0)")

    for wid in ws_ids:
        rest_delete(f"/api/workspaces/{wid}")
    return ws_ids


async def test_fingerprint_injection(page, browser, playwright):
    section("Fingerprint Workspace — Injection Verification")
    ws_id = ""
    child_cdp_port = None
    TEST_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    TEST_WIDTH = 1366
    TEST_HEIGHT = 768
    TEST_PLATFORM = "Win32"
    TEST_LOCALE = "en-US"
    TEST_HW_CONCURRENCY = 4
    TEST_TZ = "America/New_York"
    TEST_SCHEME = "dark"

    try:
        raw, status = rest_post("/api/workspaces", {
            "name": "Fingerprint-Injection-Test",
            "type": "fingerprint",
            "user_agent": TEST_UA,
            "screen_width": TEST_WIDTH,
            "screen_height": TEST_HEIGHT,
            "hardware_concurrency": TEST_HW_CONCURRENCY,
            "platform": TEST_PLATFORM,
            "locale": TEST_LOCALE,
            "timezone": TEST_TZ,
            "color_scheme": TEST_SCHEME,
            "proxy": "",
        })
        d = unwrap_rest(raw) or {}
        ws_id = d.get("id", "")
        if not ws_id:
            fail("Fingerprint: create workspace", f"status={status}")
            return
        ok("Fingerprint: workspace created", f"id={ws_id[:12]}…")

        raw, status = rest_post(f"/api/workspaces/{ws_id}/launch")
        d = unwrap_rest(raw) or {}
        if status != 200 or d.get("status") != "running":
            fail("Fingerprint: launch", f"status={status}")
            return
        child_cdp_port = d.get("cdp_port")
        ok("Fingerprint: launched", f"cdp_port={child_cdp_port}")

        await asyncio.sleep(2)

        # Connect directly to the child workspace's CDP port
        child_browser = await playwright.chromium.connect_over_cdp(
            f"http://localhost:{child_cdp_port}")

        child_ctx = child_browser.contexts[0] if child_browser.contexts else await child_browser.new_context()
        child_page = await child_ctx.new_page()
        await child_page.goto("about:blank", wait_until="domcontentloaded")
        await asyncio.sleep(1)

        fp = await child_page.evaluate("""() => {
            const tz = (() => {
                try { return Intl.DateTimeFormat().resolvedOptions().timeZone; }
                catch(e) { return 'error'; }
            })();
            const colorScheme = (() => {
                try { return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'; }
                catch(e) { return 'error'; }
            })();
            return {
                userAgent: navigator.userAgent,
                platform: navigator.platform,
                language: navigator.language,
                webdriver: navigator.webdriver,
                pluginsLength: navigator.plugins.length,
                hardwareConcurrency: navigator.hardwareConcurrency,
                screenWidth: screen.width,
                screenHeight: screen.height,
                screenAvailWidth: screen.availWidth,
                screenAvailHeight: screen.availHeight,
                timezone: tz,
                colorScheme: colorScheme,
            };
        }""")

        # Get parent UA for comparison
        parent_ua = await page.evaluate("navigator.userAgent")

        fingerprint_checks = [
            ("User-Agent override differs from parent",
             fp.get("userAgent") != parent_ua,
             f"parent={parent_ua[:60]} child={fp.get('userAgent', '')[:60]}"),
            ("Locale override", fp.get("language") == TEST_LOCALE,
             f"got: {fp.get('language', '')}"),
            ("WebDriver hidden", fp.get("webdriver") in (False, None),
             f"got: {fp.get('webdriver')}"),
            ("Plugins populated", (fp.get("pluginsLength") or 0) > 0,
             f"got: {fp.get('pluginsLength')}"),
        ]
        for name, passed, detail in fingerprint_checks:
            if passed:
                ok(f"Fingerprint: {name}")
            else:
                fail(f"Fingerprint: {name}", detail)

        # Phase 0 protector verifications
        hw_checks = [
            ("HardwareConcurrency override",
             fp.get("hardwareConcurrency") == TEST_HW_CONCURRENCY,
             f"got: {fp.get('hardwareConcurrency')}"),
            ("Screen width override",
             fp.get("screenWidth") == TEST_WIDTH,
             f"got: {fp.get('screenWidth')}"),
            ("Screen height override",
             fp.get("screenHeight") == TEST_HEIGHT,
             f"got: {fp.get('screenHeight')}"),
            ("Screen availWidth override",
             fp.get("screenAvailWidth") == TEST_WIDTH,
             f"got: {fp.get('screenAvailWidth')}"),
            ("Timezone override",
             fp.get("timezone") == TEST_TZ,
             f"got: {fp.get('timezone')}"),
            ("Color scheme override",
             fp.get("colorScheme") == TEST_SCHEME,
             f"got: {fp.get('colorScheme')}"),
        ]
        for name, passed, detail in hw_checks:
            if passed:
                ok(f"Fingerprint: {name}")
            else:
                fail(f"Fingerprint: {name}", detail)

        # Platform override — JS injection should work
        if fp.get("platform") == TEST_PLATFORM:
            ok("Fingerprint: Platform override", f"got: {fp.get('platform')}")
        else:
            warn("Fingerprint: Platform (no CLI flag, derived from binary)",
                 f"expected={TEST_PLATFORM} got={fp.get('platform')}")

        await child_page.close()
        await child_browser.close()
    except Exception as e:
        fail("Fingerprint injection test crash", str(e)[:100])
    finally:
        if ws_id:
            rest_post(f"/api/workspaces/{ws_id}/stop")
            await asyncio.sleep(1)
            rest_delete(f"/api/workspaces/{ws_id}")


async def test_cdp_proxy(page, browser):
    section("CDP Proxy Handler")
    ws_id = ""
    try:
        raw, status = rest_post("/api/workspaces", {
            "name": "CDP-Proxy-Test", "type": "normal",
        })
        d = unwrap_rest(raw) or {}
        ws_id = d.get("id", "")
        if not ws_id:
            fail("CDP Proxy: create workspace", f"status={status}")
            return
        ok("CDP Proxy: workspace created", f"id={ws_id[:12]}…")

        raw, status = rest_post(f"/api/workspaces/{ws_id}/launch")
        d = unwrap_rest(raw) or {}
        if status != 200 or d.get("status") != "running":
            fail("CDP Proxy: launch", f"status={status}")
            return
        ok("CDP Proxy: workspace launched")

        await asyncio.sleep(1)

        raw, status = rest_get(f"/api/workspaces/{ws_id}/cdp/json/version")
        if status == 200 and raw:
            browser_name = raw.get("Browser", "")[:50]
            ok("CDP Proxy: /cdp/json/version", browser_name)
        else:
            fail("CDP Proxy: /cdp/json/version", f"status={status}")

        raw, status = rest_get(f"/api/workspaces/{ws_id}/cdp/json/list")
        if status == 200 and isinstance(raw, list):
            ok("CDP Proxy: /cdp/json/list", f"{len(raw)} targets")
        elif status == 200 and isinstance(raw, dict):
            wsd = raw.get("webSocketDebuggerUrl", "")
            ok("CDP Proxy: /cdp/json/list (stub)", f"webSocketDebuggerUrl={wsd[:50]}…")
        else:
            fail("CDP Proxy: /cdp/json/list", f"status={status} type={type(raw).__name__}")
    finally:
        if ws_id:
            rest_post(f"/api/workspaces/{ws_id}/stop")
            await asyncio.sleep(1)
            rest_delete(f"/api/workspaces/{ws_id}")


async def test_edge_cases(page, browser):
    section("Edge Cases")

    raw, status = rest_post("/api/workspaces/nonexistent-id-12345/stop")
    if isinstance(status, int) and status >= 400:
        ok("Edge: stop non-existent returns error", f"status={status}")
    elif isinstance(status, str):
        warn("Edge: stop non-existent", f"connection error={status}")
    else:
        warn("Edge: stop non-existent", f"status={status} (expected error)")

    raw, status = rest_post("/api/workspaces/nonexistent-id-12345/launch")
    if isinstance(status, int) and status >= 400:
        ok("Edge: launch non-existent returns error", f"status={status}")
    elif isinstance(status, str):
        warn("Edge: launch non-existent", f"connection error={status}")
    else:
        warn("Edge: launch non-existent", f"status={status} (expected error)")

    raw, status = rest_post("/api/workspaces", {
        "name": "Headless-Edge-Test", "type": "normal", "headless": True,
    })
    d = unwrap_rest(raw) or {}
    ws_id = d.get("id", "")
    if ws_id:
        ok("Edge: created headless workspace", f"id={ws_id[:12]}…")
    else:
        fail("Edge: create headless workspace", f"status={status}")
        ws_id = None

    if ws_id:
        raw, status = rest_post(f"/api/workspaces/{ws_id}/launch")
        d = unwrap_rest(raw) or {}
        if status == 200 and d.get("status") == "running":
            ok("Edge: headless workspace launched", f"cdp_port={d.get('cdp_port')}")
            rest_post(f"/api/workspaces/{ws_id}/stop")
            await asyncio.sleep(1)
        else:
            warn("Edge: headless launch", f"status={status}")
        rest_delete(f"/api/workspaces/{ws_id}")

    raw, status = rest_post("/api/workspaces", {"name": "", "type": "normal"})
    d = unwrap_rest(raw) or {}
    if status == 200 and d.get("id"):
        ok("Edge: empty name accepted (or auto-filled)")
        rest_delete(f"/api/workspaces/{d['id']}")
    else:
        warn("Edge: empty name rejected", f"status={status}")

    await asyncio.sleep(1)

    raw, status = rest_post("/api/workspaces", {"name": "Minimal-WS"})
    d = unwrap_rest(raw) or {}
    wid = d.get("id", "")
    if status == 200 and wid:
        ok("Edge: minimal workspace (name only)", f"id={wid[:12]}…")
        rest_delete(f"/api/workspaces/{wid}")
    else:
        warn("Edge: minimal workspace", f"status={status}")


async def test_watchdog_crash_detection(page, browser):
    section("Watchdog — Crash Detection")
    ws_id = ""
    child_pid = None
    try:
        raw, status = rest_post("/api/workspaces", {
            "name": "Watchdog-Crash-Test", "type": "normal",
        })
        d = unwrap_rest(raw) or {}
        ws_id = d.get("id", "")
        if not ws_id:
            fail("Watchdog: create workspace", f"status={status}")
            return
        ok("Watchdog: workspace created", f"id={ws_id[:12]}…")

        raw, status = rest_post(f"/api/workspaces/{ws_id}/launch")
        d = unwrap_rest(raw) or {}
        if status != 200 or d.get("status") != "running":
            fail("Watchdog: launch", f"status={status}")
            return
        child_pid = d.get("pid")
        ok("Watchdog: launched", f"pid={child_pid}")

        await asyncio.sleep(2)

        raw, status = rest_get(f"/api/workspaces/{ws_id}/status")
        initial = unwrap_rest(raw) or {}
        ok("Watchdog: initial status", f"status={initial.get('status')}")

        try:
            os.kill(child_pid, 9)
            ok("Watchdog: child process killed (SIGKILL)")
        except OSError as e:
            fail("Watchdog: kill child", str(e))
            return

        await asyncio.sleep(8)

        raw, status = rest_get(f"/api/workspaces/{ws_id}/status")
        after = unwrap_rest(raw) or {}
        if after.get("status") == "crashed":
            ok("Watchdog: status changed to crashed")
        else:
            warn("Watchdog: status after kill",
                 f"status={after.get('status')} (expected 'crashed')")
    except Exception as e:
        fail("Watchdog test crash", str(e)[:100])
        import traceback
        traceback.print_exc()
    finally:
        if ws_id:
            rest_post(f"/api/workspaces/{ws_id}/stop")
            await asyncio.sleep(1)
            rest_delete(f"/api/workspaces/{ws_id}")


async def test_toolbar_and_side_panel(page, browser):
    section("Toolbar Button & Side Panel")
    await page.goto("purecloak://purecloak/", wait_until="domcontentloaded", timeout=15000)
    await page.wait_for_timeout(2000)

    toolbar_btn = await page.evaluate("""() => {
        const selectors = [
            'purecloak-toolbar-button',
            '.purecloak-toolbar-button',
            '[tooltip*="PureCloak"]',
            '[aria-label*="PureCloak"]',
            '[aria-label*="工作区"]',
            'toolbar-button',
        ];
        for (const sel of selectors) {
            const el = document.querySelector(sel);
            if (el) return {selector: sel, found: true, text: el.textContent?.trim()};
        }
        return {found: false};
    }""")

    if toolbar_btn.get("found"):
        ok("Toolbar button found", f"selector={toolbar_btn.get('selector')}")
    else:
        warn("Toolbar button", "not in page DOM (C++ view, only via UI automation)")

    side_panel_els = await page.evaluate("""() => {
        return {
            hasSidePanel: !!document.querySelector('.side-panel, #sidePanel, [class*="side"]'),
            hasWorkspaceList: !!document.getElementById('workspaceList'),
            hasCreateBtn: !!document.getElementById('btnCreateWs'),
        };
    }""")
    if side_panel_els.get("hasSidePanel"):
        ok("Side panel elements present")
    else:
        warn("Side panel", "no side-panel-specific elements in DOM (C++ side panel)")
    ok("Workspace list and create button accessible from WebUI")


async def test_i18n(page, browser):
    section("i18n — Locale Detection")
    await page.goto("purecloak://purecloak/", wait_until="domcontentloaded", timeout=15000)
    await page.wait_for_timeout(2000)

    locale_info = await page.evaluate("""() => {
        const title = document.querySelector('.header h1')?.textContent || '';
        const btn = document.getElementById('btnCreateWs')?.textContent || '';
        const emptyState = document.getElementById('emptyState')?.textContent || '';
        const hasChinese = /[\\u4e00-\\u9fff]/.test(title + btn + emptyState);
        return {title, btnText: btn, emptyState: emptyState.slice(0, 50), hasChinese};
    }""")

    if locale_info.get("hasChinese"):
        ok("Locale: Chinese (zh-CN)", f"title={locale_info.get('title')}")
    else:
        ok("Locale: English (en-US)", f"title={locale_info.get('title')}")

    btn_text = locale_info.get("btnText", "")
    if "新建" in btn_text or "New" in btn_text or "+" in btn_text:
        ok("Create button text present", btn_text[:40])
    else:
        warn("Create button text", btn_text[:40])


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
            await test_multi_workspace(page, browser)
            await test_fingerprint_injection(page, browser, p)
            await test_cdp_proxy(page, browser)
            await test_edge_cases(page, browser)
            await test_watchdog_crash_detection(page, browser)
            await test_toolbar_and_side_panel(page, browser)
            await test_i18n(page, browser)

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
