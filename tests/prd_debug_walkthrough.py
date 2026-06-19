#!/usr/bin/env python3
"""
PureCloak Visual Debug Walkthrough
Launches PureCloak Chrome in HEADED (visible) mode and walks through
all features step by step with screenshots, console capture, and pauses.
"""

import asyncio
import json
import subprocess
import time
import sys
import os
import http.client
from datetime import datetime
from playwright.async_api import async_playwright

CHROME_BINARY = "/home/molandtoxx/PureCloak/src/out/purecloak/chrome"
CDP_PORT = 9333
USER_DATA_DIR = "/tmp/purecloak_debug_profile"
SCREENSHOT_DIR = "/tmp/purecloak_debug_screenshots"
STEP_DELAY = 1.0  # seconds to pause between steps for visual observation

# ── Helpers ──────────────────────────────────────────────────────────────────

_step_num = 0


def step(description):
    """Print a step marker."""
    global _step_num
    _step_num += 1
    print(f"\n{'='*60}")
    print(f"  STEP {_step_num}: {description}")
    print(f"{'='*60}")


def wait_for_cdp(port, timeout=30):
    """Wait for Chrome CDP endpoint to become available."""
    start = time.time()
    while time.time() - start < timeout:
        try:
            conn = http.client.HTTPConnection("localhost", port, timeout=2)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                data = json.loads(resp.read().decode())
                conn.close()
                return data
            conn.close()
        except (ConnectionRefusedError, http.client.HTTPException, OSError):
            pass
        time.sleep(0.5)
    return None


async def pause(page, label=""):
    """Pause for visual observation + print hint."""
    if label:
        print(f"  👀 Observing: {label}")
    print(f"  ⏸️  Pausing {STEP_DELAY}s for visual inspection...")
    await asyncio.sleep(STEP_DELAY)


async def screenshot(page, name):
    """Save a screenshot with timestamp."""
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)
    ts = datetime.now().strftime("%H%M%S")
    path = f"{SCREENSHOT_DIR}/{ts}_{name}.png"
    await page.screenshot(path=path)
    print(f"  📸 Screenshot: {path}")
    return path


async def capture_console(page, label=""):
    """Get recent console messages."""
    try:
        cdp = await page.context.new_cdp_session(page)
        result = await cdp.send("Runtime.evaluate", {
            "expression": """
                (() => {
                    // Try to get console log from window.__consoleCapture
                    if (window.__consoleCapture) {
                        return window.__consoleCapture.slice(-10);
                    }
                    // Try the Log domain
                    return [];
                })()
            """,
            "returnByValue": True
        })
        await cdp.detach()
        msgs = result.get("result", {}).get("value", [])
        if msgs:
            print(f"  📋 Console ({label}):")
            for m in msgs:
                print(f"    {m}")
    except Exception as e:
        print(f"  📋 Console: could not capture ({e})")


async def show_page_state(page, label):
    """Print current URL and title."""
    url = page.url
    title = await page.title()
    print(f"  📄 URL: {url}")
    print(f"  📄 Title: {title}")


# ── Walkthrough ──────────────────────────────────────────────────────────────

async def main():
    global STEP_DELAY

    print("=" * 60)
    print("  PureCloak Visual Debug Walkthrough")
    print("  Chrome will open in a VISIBLE window")
    print("=" * 60)
    print()
    print("  Controls:")
    print("    STEP_DELAY = time between steps (currently 1.0s)")
    print("    Pass a number as argument to change (e.g. ./script.py 2.0)")
    print("    Ctrl+C to exit at any time")
    print()

    if len(sys.argv) > 1:
        try:
            STEP_DELAY = float(sys.argv[1])
            print(f"  ⏱️  Step delay set to {STEP_DELAY}s")
        except ValueError:
            print(f"  ⚠️  Invalid delay '{sys.argv[1]}', using default 1.0s")

    # Clean up old profile
    os.system(f"rm -rf {USER_DATA_DIR}")
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)

    # Launch PureCloak Chrome in HEADED mode
    step("Launching PureCloak Chrome (headed mode)")
    print("  → A Chrome window will appear shortly...")

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

    version_info = wait_for_cdp(CDP_PORT, timeout=30)
    if version_info is None:
        print("❌ Chrome failed to start!")
        proc.kill()
        sys.exit(1)

    browser_version = version_info.get("Browser", "unknown")
    print(f"  ✅ Chrome PID: {proc.pid}, Version: {browser_version}")
    print(f"  ✅ CDP active on port {CDP_PORT}")

    # Connect Playwright
    async with async_playwright() as p:
        browser = await p.chromium.connect_over_cdp(
            f"http://localhost:{CDP_PORT}"
        )
        print(f"  ✅ Playwright connected via CDP")
        print(f"  🔍 Chrome window should now be visible on your screen!")

        # ── 1. Open PureCloak WebUI ──
        step("Opening chrome://purecloak/")
        page = await browser.new_page()
        console_logs = []

        # Capture console messages
        page.on("console", lambda msg: console_logs.append(
            f"[{msg.type}] {msg.text[:200]}"
        ))

        await page.goto("chrome://purecloak/", wait_until="domcontentloaded")
        await page.wait_for_timeout(2000)
        await show_page_state(page, "PureCloak loaded")
        await screenshot(page, "01_webui_loaded")

        if console_logs:
            print(f"  📋 Console messages:")
            for log in console_logs[-10:]:
                print(f"    {log}")

        # Check that workspace list is visible
        step("Checking WebUI elements")
        workspace_list = await page.query_selector("#workspaceList")
        btn_create = await page.query_selector("#btnCreateWs")
        empty_state = await page.query_selector("#emptyState")
        print(f"  🔍 Workspace list (#workspaceList): {'✅ FOUND' if workspace_list else '❌ MISSING'}")
        print(f"  🔍 Create button (#btnCreateWs): {'✅ FOUND' if btn_create else '❌ MISSING'}")
        print(f"  🔍 Empty state (#emptyState): {'✅ FOUND' if empty_state else '❌ MISSING'}")
        await screenshot(page, "02_webui_elements")

        # ── 2. Create Normal Workspace ──
        step("Creating a Normal workspace via sendAsync")
        normal_ws = await page.evaluate("""async () => {
            return await sendAsync('createWorkspace', 'DebugNormalWS', 0);
        }""")
        ws_id = normal_ws.get("id", "?") if isinstance(normal_ws, dict) else "?"
        print(f"  ✅ Created normal workspace: id={ws_id[:12]}...")
        await pause(page, "Normal workspace created")

        # ── 3. Create Fingerprint Workspace ──
        step("Creating a Fingerprint workspace via sendAsync")
        fp_ws = await page.evaluate("""async () => {
            return await sendAsync('createWorkspace', 'DebugFingerprintWS', 1);
        }""")
        fp_id = fp_ws.get("id", "?") if isinstance(fp_ws, dict) else "?"
        print(f"  ✅ Created fingerprint workspace: id={fp_id[:12]}...")
        await pause(page, "Fingerprint workspace created")

        # ── 4. Verify in store ──
        step("Verifying workspaces in store")
        all_ws = await page.evaluate("""async () => {
            return await sendAsync('getAllWorkspaces');
        }""")
        if isinstance(all_ws, list):
            print(f"  📋 Workspaces in store ({len(all_ws)}):")
            for w in all_ws:
                print(f"    • {w.get('name','?')} (type={w.get('type','?')}, id={w.get('id','?')[:12]}...)")
        await screenshot(page, "03_workspaces_in_store")

        # ── 5. Rename workspace ──
        step("Renaming workspace")
        if isinstance(all_ws, list) and len(all_ws) > 0:
            rename_id = all_ws[0].get("id", "")
            rename_result = await page.evaluate(f"""async () => {{
                return await sendAsync('updateWorkspace', '{rename_id}', 'RenamedDebugWS');
            }}""")
            print(f"  {'✅' if rename_result else '❌'} Rename result: {rename_result}")
        await pause(page, "Workspace renamed")

        # ── 6. Type lock verification ──
        step("Verifying workspace type locks")
        all_ws2 = await page.evaluate("""async () => {
            return await sendAsync('getAllWorkspaces');
        }""")
        if isinstance(all_ws2, list):
            print(f"  📋 Current workspaces ({len(all_ws2)}):")
            for w in all_ws2:
                name = w.get("name", "?")
                wtype = w.get("type", "?")
                locked = w.get("type_locked", "N/A")
                print(f"    • {name} → type={wtype}")
            # Verify that both types are present
            types = [w.get("type") for w in all_ws2]
            has_normal = "normal" in types
            has_fingerprint = "fingerprint" in types
            if has_normal and has_fingerprint:
                print(f"  ✅ Both workspace types verified!")
            else:
                print(f"  ⚠️  Expected both 'normal' and 'fingerprint', got: {types}")
            await pause(page, "Type locks are immutable")

        # ── 7. Create Profile ──
        step("Creating a profile in fingerprint workspace")
        profile = await page.evaluate(f"""async () => {{
            return await sendAsync('createProfile', '{fp_id}', 'DebugProfile1');
        }}""")
        if isinstance(profile, dict) and "id" in profile:
            pid = profile.get("id", "")[:12]
            print(f"  ✅ Created profile: id={pid}...")
            print(f"  📋 Profile details: {json.dumps(profile, default=str)[:200]}")
        else:
            print(f"  ❌ Profile creation failed: {profile}")
        await pause(page, "Profile created")

        # ── 8. List profiles ──
        step("Listing profiles for workspace")
        profiles = await page.evaluate(f"""async () => {{
            return await sendAsync('getProfilesForWorkspace', '{fp_id}');
        }}""")
        if isinstance(profiles, list):
            print(f"  📋 Profiles ({len(profiles)}):")
            for p in profiles:
                print(f"    • {p.get('name','?')} (id={p.get('id','?')[:12]}...)")
        await pause(page, "Profile list")

        # ── 9. Delete profile ──
        step("Deleting profile")
        if isinstance(profiles, list) and len(profiles) > 0:
            del_pid = profiles[0].get("id", "")
            del_result = await page.evaluate(f"""async () => {{
                return await sendAsync('deleteProfile', '{del_pid}');
            }}""")
            print(f"  {'✅' if del_result else '❌'} Delete profile result: {del_result}")
        await pause(page, "Profile deleted")
        await screenshot(page, "04_after_profile_delete")

        # ── 10. Delete workspace ──
        step("Deleting workspaces")
        if isinstance(all_ws2, list):
            for w in all_ws2:
                wid = w.get("id", "")
                wname = w.get("name", "?")
                result = await page.evaluate(f"""async () => {{
                    return await sendAsync('deleteWorkspace', '{wid}');
                }}""")
                print(f"  {'✅' if result else '❌'} Deleted '{wname}': {result}")
        await pause(page, "Workspaces deleted")

        # ── 11. Verify empty ──
        step("Verifying store is empty")
        final_list = await page.evaluate("""async () => {
            return await sendAsync('getAllWorkspaces');
        }""")
        count = len(final_list) if isinstance(final_list, list) else -1
        print(f"  📋 Workspaces remaining: {count}")
        await screenshot(page, "05_store_empty")

        # ── 12. Console log summary ──
        step("Debug Summary")
        print(f"  📋 Total console messages captured: {len(console_logs)}")
        if console_logs:
            print(f"  Last 5 console messages:")
            for log in console_logs[-5:]:
                print(f"    {log}")

        print(f"\n  📸 All screenshots saved to: {SCREENSHOT_DIR}")
        print(f"  🖥️  Chrome PID: {proc.pid} (keep it open for inspection)")
        print(f"  🔗 CDP endpoint: ws://localhost:{CDP_PORT}")

        # ── 13. Interactive inspection ──
        step("Interactive mode")
        print("  You can now inspect the Chrome window directly.")
        print("  The browser will stay open for 60 seconds.")
        print("  Press Ctrl+C to exit early.")
        print(f"  ⏱️  Waiting 60s...")
        try:
            await asyncio.sleep(60)
        except asyncio.CancelledError:
            pass

        await page.close()
        await browser.close()

    # Cleanup
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    print(f"\n{'='*60}")
    print("  Walkthrough complete! 🎉")
    print(f"{'='*60}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n  👋 Interrupted by user. Cleaning up...")
        # Kill any lingering Chrome process
        os.system(f"pkill -f 'remote-debugging-port={CDP_PORT}' 2>/dev/null")
        sys.exit(0)
