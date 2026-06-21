#!/usr/bin/env python3
"""Shared test infrastructure for PureCloak comprehensive test suite."""

import json
import subprocess
import time
import sys
import os
import http.client
import urllib.request
import urllib.error

# ── Config ──────────────────────────────────────────────────────────────────
CHROME_BINARY = "/home/molandtoxx/PureCloak/src/out/purecloak/chrome"
CDP_PORT = 9233
API_PORT = 9334
USER_DATA_DIR = "/tmp/purecloak_debug_session"
SCREENSHOT_DIR = "/tmp/purecloak_debug_screenshots"
REST_BASE = f"http://127.0.0.1:{API_PORT}"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

PASS = 0
FAIL = 0
WARN = 0


def ok(name, detail=""):
    global PASS
    PASS += 1
    icon = "✅"
    print(f"  {icon} {name}" + (f" — {detail[:120]}" if detail else ""))


def fail(name, detail=""):
    global FAIL
    FAIL += 1
    icon = "❌"
    print(f"  {icon} {name}" + (f" — {str(detail)[:200]}" if detail else ""))


def warn(name, detail=""):
    global WARN
    WARN += 1
    icon = "⚠️"
    print(f"  {icon} {name}" + (f" — {str(detail)[:200]}" if detail else ""))


def section(title):
    print(f"\n{'=' * 60}")
    print(f"  📋 {title}")
    print(f"{'=' * 60}")


# ── REST API helpers ────────────────────────────────────────────────────────

def rest_get(path):
    try:
        r = urllib.request.urlopen(f"{REST_BASE}{path}", timeout=5)
        return json.loads(r.read().decode()), r.status
    except Exception as e:
        return None, str(e)


def rest_post(path, body=None):
    try:
        data = json.dumps(body).encode() if body else b""
        r = urllib.request.urlopen(
            urllib.request.Request(
                f"{REST_BASE}{path}", data=data,
                headers={"Content-Type": "application/json"},
                method="POST"),
            timeout=5)
        return json.loads(r.read().decode()), r.status
    except Exception as e:
        return None, str(e)


def rest_put(path, body=None):
    try:
        data = json.dumps(body).encode() if body else b""
        r = urllib.request.urlopen(
            urllib.request.Request(
                f"{REST_BASE}{path}", data=data,
                headers={"Content-Type": "application/json"},
                method="PUT"),
            timeout=5)
        return json.loads(r.read().decode()), r.status
    except Exception as e:
        return None, str(e)


def rest_delete(path):
    try:
        r = urllib.request.urlopen(
            urllib.request.Request(f"{REST_BASE}{path}", method="DELETE"),
            timeout=5)
        return json.loads(r.read().decode()), r.status
    except Exception as e:
        return None, str(e)


def unwrap_rest(data):
    if data is None:
        return None
    if isinstance(data, dict) and "data" in data:
        return data["data"]
    return data


# ── Chrome launcher ─────────────────────────────────────────────────────────

def launch_chrome():
    section("Launching PureCloak")
    os.system(f"rm -rf {USER_DATA_DIR}")
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
            "--window-size=1400,900",
            f"--purecloak-api-port={API_PORT}",
            "about:blank",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    start = time.time()
    while time.time() - start < 30:
        try:
            conn = http.client.HTTPConnection("localhost", CDP_PORT, timeout=2)
            conn.request("GET", "/json/version")
            resp = conn.getresponse()
            if resp.status == 200:
                version = json.loads(resp.read().decode())
                conn.close()
                print(f"  PID: {proc.pid}")
                print(f"  Browser: {version.get('Browser', 'unknown')}")
                print(f"  CDP: ws://localhost:{CDP_PORT}")
                print(f"  REST API: {REST_BASE}")
                ok("Chrome launched visibly")
                return proc, version
            conn.close()
        except Exception:
            pass
        time.sleep(0.5)
    fail("Chrome failed to start (CDP timeout)")
    proc.kill()
    sys.exit(1)


def wait_for_api():
    section("REST API Server")
    start = time.time()
    while time.time() - start < 20:
        data, status = rest_get("/api/status")
        if status == 200 and data:
            ok("API /api/status", json.dumps(data, ensure_ascii=False)[:120])
            print(f"    Full: {json.dumps(data, indent=2, ensure_ascii=False)[:300]}")
            return data
        time.sleep(0.5)
    fail("API server not available after 20s")
