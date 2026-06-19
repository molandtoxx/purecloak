#!/usr/bin/env python3
"""
PureCloak Visual E2E Test - Launches visible Chrome and verifies workspace creation + fingerprint injection
"""
import asyncio
import json
import subprocess
import time
import sys
import os
import http.client
from playwright.async_api import async_playwright

CHROME_BINARY = "/home/molandtoxx/PureCloak/chromium_src/out/purecloak/chrome"
CDP_PORT = 9333
USER_DATA_DIR = "/tmp/purecloak_visual_test"
SCREENSHOT_DIR = "/tmp/purecloak_screenshots"

async def main():
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)
    os.system(f"rm -rf {USER_DATA_DIR}")

    # ── Launch PureCloak Chrome VISIBLE (no --headless) ──
    print("🚀 Launching PureCloak Chrome (VISIBLE)...")
    proc = subprocess.Popen(
        [
            CHROME_BINARY,
            f"--remote-debugging-port={CDP_PORT}",
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

    # Wait for CDP
    start = time.time()
    version_info = None
    while time.time() - start < 30:
        try:
            conn = http.client.HTTPConnection("localhost", CDP_PORT, timeout=2)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                version_info = json.loads(resp.read().decode())
                conn.close()
                break
            conn.close()
        except Exception:
            pass
        time.sleep(0.5)

    if version_info is None:
        print("❌ Chrome failed to start (CDP not available after 30s)")
        proc.kill()
        sys.exit(1)

    print(f"  PID: {proc.pid}")
    print(f"  Browser: {version_info.get('Browser', 'unknown')}")
    print(f"  CDP: ws://localhost:{CDP_PORT}")

    async with async_playwright() as p:
        browser = await p.chromium.connect_over_cdp(f"http://localhost:{CDP_PORT}")
        context = browser.contexts[0]
        page = await context.new_page()

        try:
            # ── Screenshot 1: chrome://purecloak/ ──
            print("\n📸 Screenshot 1: chrome://purecloak/")
            await page.goto("chrome://purecloak/", wait_until="networkidle", timeout=15000)
            await page.wait_for_timeout(3000)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/01_purecloak_webui.png", full_page=True)
            print(f"  Saved to {SCREENSHOT_DIR}/01_purecloak_webui.png")

            # ── Create a workspace via WebUI ──
            print("\n🔧 Creating workspace via sendAsync...")
            ws_result = await page.evaluate("""async () => {
                return await sendAsync('createWorkspace', 'VisualTestWS', 0);
            }""")
            ws_id = ws_result.get("id", "") if isinstance(ws_result, dict) else ""
            print(f"  Workspace created: id={ws_id[:16] if ws_id else 'FAILED'}")

            await page.wait_for_timeout(2000)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/02_workspace_created.png", full_page=True)

            # ── Screenshot 2: about:blank with fingerprint checks ──
            print("\n📸 Screenshot 2: Fingerprint check on about:blank")
            await page.goto("about:blank", wait_until="domcontentloaded", timeout=10000)

            # Run comprehensive fingerprint checks
            results = await page.evaluate("""() => {
                const r = {};
                r.webdriver = navigator.webdriver;
                r.plugins_length = navigator.plugins.length;
                r.languages = navigator.languages;
                r.platform = navigator.platform;
                r.hardwareConcurrency = navigator.hardwareConcurrency;
                r.screen_width = screen.width;
                r.screen_height = screen.height;
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

            for k, v in results.items():
                status = "✅" if v not in (None, "undefined", False) else "❌"
                print(f"  {status} {k}: {str(v)[:80]}")

            # Screenshot the fingerprint test results
            await page.evaluate(f"""() => {{
                const d = document.createElement('div');
                d.style.cssText = 'padding:20px;font-family:monospace;font-size:14px;background:#fff;color:#000;';
                d.innerHTML = '<h2>PureCloak Fingerprint Injection Test</h2><table border="1" cellpadding="6">' +
                    Object.entries({json.dumps(results)}).map(([k,v]) =>
                        '<tr><td><b>' + k + '</b></td><td>' + JSON.stringify(v) + '</td></tr>'
                    ).join('') + '</table>';
                document.body.innerHTML = '';
                document.body.appendChild(d);
            }}""")
            await page.wait_for_timeout(1000)
            await page.screenshot(path=f"{SCREENSHOT_DIR}/03_fingerprint_test.png", full_page=True)
            print(f"  Saved to {SCREENSHOT_DIR}/03_fingerprint_test.png")

            # ── Verify webdriver IS hidden (most critical check) ──
            webdriver_val = results.get("webdriver")
            webdriver_hidden = webdriver_val in (False, None)
            print(f"\n🔑 CRITICAL CHECK: navigator.webdriver = {webdriver_val} {'✅ HIDDEN' if webdriver_hidden else '❌ EXPOSED'}")

            # ── Cleanup ──
            print(f"\n📸 Screenshots saved to {SCREENSHOT_DIR}/")
            os.system(f"ls -la {SCREENSHOT_DIR}/")

        except Exception as e:
            print(f"\n❌ Test error: {e}")
            import traceback
            traceback.print_exc()
            # Take error screenshot
            try:
                await page.screenshot(path=f"{SCREENSHOT_DIR}/error.png", full_page=True)
            except:
                pass

        finally:
            await page.close()
            await browser.close()

    # Cleanup
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

    print("\n✨ Visual E2E test complete!")

if __name__ == "__main__":
    asyncio.run(main())
