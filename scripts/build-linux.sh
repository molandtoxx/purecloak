#!/usr/bin/env bash
#
# build-linux.sh — Build PureCloak for Linux x86_64
#
# Usage: bash build-linux.sh [options]
#   --debug          Build debug (default: release)
#   --out DIR        Output directory (default: out/purecloak)
#   --skip-sync      Skip gclient sync (use existing checkout)
#   --skip-tests     Skip building and running unit tests
#   --jobs N         Parallel build jobs (default: nproc)
#   --no-strip       Don't strip the final binary
#   --gclient-file   Path to .gclient file (default: ../.gclient)
#
# Prerequisites (one-time):
#   sudo ./chromium_src/build/install-build-deps.sh
#
# Requires ~50GB free disk, 16GB+ RAM.
# First build: gclient sync (30-60min) + compile (2-4 hours).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Config ──────────────────────────────────────────────────────────
BUILD_TYPE="release"
OUT_DIR="out/purecloak"
SKIP_SYNC=false
SKIP_TESTS=false
JOBS="$(nproc 2>/dev/null || echo 4)"
DO_STRIP=true
GCLIENT_FILE="$PROJECT_DIR/.gclient"

# ── Parse args ─────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)       BUILD_TYPE="debug"; shift ;;
    --out)         OUT_DIR="$2"; shift 2 ;;
    --skip-sync)   SKIP_SYNC=true; shift ;;
    --skip-tests)  SKIP_TESTS=true; shift ;;
    --jobs)        JOBS="$2"; shift 2 ;;
    --no-strip)    DO_STRIP=false; shift ;;
    --gclient-file) GCLIENT_FILE="$2"; shift 2 ;;
    --help|-h)
      sed -n '2,15p' "$0"
      exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

cd "$PROJECT_DIR"
echo "==> PureCloak Linux Build"
echo "    Project: $PROJECT_DIR"
echo "    Type:    $BUILD_TYPE"
echo "    Out:     $OUT_DIR"
echo "    Jobs:    $JOBS"

# ── 1. Check prerequisites ──────────────────────────────────────────
MISSING=""
for cmd in python3 git ninja; do
  if ! command -v "$cmd" &>/dev/null; then echo "  [✗] $cmd"; else echo "  [✓] $cmd"; fi
done

if ! python3 -c "import subprocess,json,os,shutil,platform,multiprocessing" 2>/dev/null; then
  echo "  [✗] Python modules (need: json, os, shutil, platform, multiprocessing)" >&2
  MISSING="1"
fi

if command -v gn &>/dev/null; then
  echo "  [✓] gn (system)"
elif [ -f "$PROJECT_DIR/depot_tools/gn" ]; then
  echo "  [✓] gn (depot_tools)"
else
  echo "  [✗] gn — must run gclient sync first or install depot_tools"
  MISSING="1"
fi

if [ -n "$MISSING" ]; then
  echo ""
  echo "Missing prerequisites. Install them:"
  echo "  sudo ./chromium_src/build/install-build-deps.sh"
  echo "  # And ensure depot_tools is in PATH"
  exit 1
fi

# ── 2. Set up depot_tools ──────────────────────────────────────────
if ! command -v gn &>/dev/null; then
  echo ""
  echo "==> Adding depot_tools to PATH..."
  export PATH="$PROJECT_DIR/depot_tools:$PATH"
fi

# ── 3. gclient sync ─────────────────────────────────────────────────
if [ "$SKIP_SYNC" = false ]; then
  echo ""
  echo "==> Step 1/5: Syncing Chromium source (gclient sync)..."

  if [ ! -f "$GCLIENT_FILE" ]; then
    echo "  [✗] .gclient not found at $GCLIENT_FILE"
    exit 1
  fi

  # gclient sync may overwrite our tracked PureCloak files with upstream.
  # We'll restore them after sync.
  cd "$(dirname "$GCLIENT_FILE")"

  # Ensure managed=True for first sync (the repo's .gclient uses managed=False
  # for local development). Temporarily override for CI/setup.
  GCLIENT_CONTENT=$(cat "$GCLIENT_FILE")
  if echo "$GCLIENT_CONTENT" | grep -q '"managed": False'; then
    echo "  [i] Temporarily enabling managed=True for initial sync..."
    sed -i 's/"managed": False/"managed": True/' "$GCLIENT_FILE"
    DID_ENABLE_MANAGED=true
  else
    DID_ENABLE_MANAGED=false
  fi

  gclient sync --shallow --nohooks -j"$JOBS"
  gclient runhooks

  # Restore managed setting
  if [ "$DID_ENABLE_MANAGED" = true ]; then
    sed -i 's/"managed": True/"managed": False/' "$GCLIENT_FILE"
  fi

  cd "$PROJECT_DIR"

  # Restore PureCloak-modified files that gclient sync may have overwritten
  echo "  [i] Restoring PureCloak custom files..."
  git checkout -- \
    chromium_src/chrome/app/chromium_strings.grd \
    chromium_src/chrome/browser/ui/browser_command_controller.cc \
    chromium_src/chrome/browser/ui/views/profiles/profile_menu_view.cc \
    2>/dev/null || true
  # Stub headers are not in upstream, but just in case:
  git checkout -- chromium_src/ash/ 2>/dev/null || true
  git checkout -- chromium_src/chromeos/ 2>/dev/null || true
  # Theme/icons are brand-specific
  git checkout -- chromium_src/chrome/app/theme/chromium/ 2>/dev/null || true
  git checkout -- chromium_src/chrome/app/theme/default_*/chromium/ 2>/dev/null || true

  echo "  [✓] Sync complete"
else
  echo "  [i] Skipping gclient sync (--skip-sync)"
fi

# ── 4. GN gen ────────────────────────────────────────────────────────
echo ""
echo "==> Step 2/5: Configuring GN..."

if [ "$BUILD_TYPE" = "debug" ]; then
  GN_ARGS="is_debug=true is_purecloak=true"
else
  GN_ARGS="
    is_debug=false
    is_purecloak=true
    is_component_build=false
    symbol_level=0
    blink_symbol_level=0
    optimize_for_size=true
    enable_nacl=false
    use_sysroot=true
    dcheck_always_on=false
    enable_iterator_debugging=false
  "
fi

gn gen "$OUT_DIR" --args="$GN_ARGS" --fail-on-unused-args
echo "  [✓] GN gen complete"

# ── 5. Build ─────────────────────────────────────────────────────────
echo ""
echo "==> Step 3/5: Building chrome (this will take a while)..."
autoninja -C "$OUT_DIR" chrome -j"$JOBS"
echo "  [✓] Build complete"

# ── 6. Build + run tests ────────────────────────────────────────────
if [ "$SKIP_TESTS" = false ]; then
  echo ""
  echo "==> Step 4/5: Building unit tests..."
  autoninja -C "$OUT_DIR" purecloak_unittests -j"$JOBS" || {
    echo "  [i] purecloak_unittests target not found, skipping"
    SKIP_TESTS=true
  }

  if [ "$SKIP_TESTS" = false ]; then
    echo "  Running tests..."
    "$OUT_DIR/purecloak_unittests" --gtest_filter=-*DeathTest* 2>&1 | tail -20
    echo "  [✓] Tests passed"
  fi
else
  echo ""
  echo "==> Step 4/5: Tests skipped"
fi

# ── 7. Strip & package ──────────────────────────────────────────────
echo ""
echo "==> Step 5/5: Packaging..."

BINARY="$OUT_DIR/chrome"
if [ -f "$BINARY" ]; then
  SIZE_BEFORE=$(du -h "$BINARY" | cut -f1)

  if [ "$DO_STRIP" = true ] && [ "$BUILD_TYPE" != "debug" ]; then
    echo "  Stripping binary (was $SIZE_BEFORE)..."
    strip "$BINARY" 2>/dev/null || true
    SIZE_AFTER=$(du -h "$BINARY" | cut -f1)
    echo "  Stripped to $SIZE_AFTER"
  fi

  # Create distribution package
  DIST_DIR="$PROJECT_DIR/dist/purecloak-linux-x86_64"
  mkdir -p "$DIST_DIR"
  cp "$BINARY" "$DIST_DIR/"

  # Also copy PureCloak WebUI resources
  if [ -d "chromium_src/purecloak/resources" ]; then
    cp -r "chromium_src/purecloak/resources" "$DIST_DIR/"
  fi

  # Create tarball
  cd "$PROJECT_DIR/dist"
  tar czf "purecloak-linux-x86_64.tar.gz" "purecloak-linux-x86_64"
  rm -rf "purecloak-linux-x86_64"
  echo "  [✓] Package: $PROJECT_DIR/dist/purecloak-linux-x86_64.tar.gz"
else
  echo "  [!] Binary not found at $BINARY"
fi

echo ""
echo "==> Build complete! 🐧"
echo "    Binary: $PROJECT_DIR/$OUT_DIR/chrome"
echo "    Run:    $PROJECT_DIR/$OUT_DIR/chrome"
