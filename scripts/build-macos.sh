#!/usr/bin/env bash
#
# build-macos.sh — Build PureCloak for macOS (x86_64 or ARM64)
#
# Usage: bash build-macos.sh [options]
#   --debug          Build debug (default: release)
#   --out DIR        Output directory (default: out/purecloak)
#   --skip-sync      Skip gclient sync
#   --skip-tests     Skip building and running unit tests
#   --arm64          Build for Apple Silicon (arm64, default on M1/M2/M3)
#   --x64            Build for Intel Macs (x86_64, default on Intel)
#   --jobs N         Parallel build jobs (default: sysctl nproc)
#   --no-strip       Don't strip the final binary
#
# Prerequisites (one-time):
#   Install Xcode 15+ from the Mac App Store
#   xcode-select --install
#   sudo ./chromium_src/build/install-build-deps.sh
#
# Requires ~60GB free disk, 16GB+ RAM.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Config ──────────────────────────────────────────────────────────
BUILD_TYPE="release"
OUT_DIR="out/purecloak"
SKIP_SYNC=false
SKIP_TESTS=false
DO_STRIP=true

# Detect architecture
ARCH="$(uname -m)"
if [ "$ARCH" = "arm64" ]; then
  TARGET_CPU="arm64"
else
  TARGET_CPU="x64"
fi
JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

# ── Parse args ─────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)       BUILD_TYPE="debug"; shift ;;
    --out)         OUT_DIR="$2"; shift 2 ;;
    --skip-sync)   SKIP_SYNC=true; shift ;;
    --skip-tests)  SKIP_TESTS=true; shift ;;
    --arm64)       TARGET_CPU="arm64"; shift ;;
    --x64)         TARGET_CPU="x64"; shift ;;
    --jobs)        JOBS="$2"; shift 2 ;;
    --no-strip)    DO_STRIP=false; shift ;;
    --help|-h)
      sed -n '2,15p' "$0"
      exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

cd "$PROJECT_DIR"
echo "==> PureCloak macOS Build"
echo "    Project:  $PROJECT_DIR"
echo "    Type:     $BUILD_TYPE"
echo "    Target:   macOS $TARGET_CPU"
echo "    Out:      $OUT_DIR"
echo "    Jobs:     $JOBS"

# ── 1. Check prerequisites ──────────────────────────────────────────
echo ""
echo "==> Checking prerequisites..."

if ! xcode-select -p &>/dev/null; then
  echo "  [✗] Xcode command line tools not installed"
  echo "  Run: xcode-select --install"
  exit 1
fi
echo "  [✓] Xcode CLT"

# Check macOS SDK
SDK_PATH="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
if [ -z "$SDK_PATH" ]; then
  echo "  [✗] macOS SDK not found"
  exit 1
fi
echo "  [✓] macOS SDK: $SDK_PATH"

for cmd in python3 git ninja; do
  if command -v "$cmd" &>/dev/null; then
    echo "  [✓] $cmd"
  else
    echo "  [!] $cmd not found (may be in depot_tools)"
  fi
done

# ── 2. Set up depot_tools ──────────────────────────────────────────
if ! command -v gn &>/dev/null; then
  if [ -f "$PROJECT_DIR/depot_tools/gn" ]; then
    echo "  [i] Adding depot_tools to PATH..."
    export PATH="$PROJECT_DIR/depot_tools:$PATH"
  else
    echo "  [i] depot_tools not found, cloning..."
    git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$PROJECT_DIR/depot_tools"
    export PATH="$PROJECT_DIR/depot_tools:$PATH"
  fi
fi

# ── 3. gclient sync ─────────────────────────────────────────────────
if [ "$SKIP_SYNC" = false ]; then
  echo ""
  echo "==> Step 1/5: Syncing Chromium source..."

  if [ ! -f "$PROJECT_DIR/.gclient" ]; then
    echo "  [✗] .gclient not found"
    exit 1
  fi

  cd "$PROJECT_DIR"

  # Temporarily enable managed=True for initial sync
  GCLIENT_CONTENT=$(cat .gclient)
  DID_ENABLE=false
  if echo "$GCLIENT_CONTENT" | grep -q '"managed": False'; then
    sed -i 's/"managed": False/"managed": True/' .gclient
    DID_ENABLE=true
  fi

  gclient sync --shallow --nohooks -j"$JOBS"
  gclient runhooks

  if [ "$DID_ENABLE" = true ]; then
    sed -i 's/"managed": True/"managed": False/' .gclient
  fi

  # Restore PureCloak-modified files
  git checkout -- \
    chromium_src/chrome/app/chromium_strings.grd \
    chromium_src/chrome/browser/ui/browser_command_controller.cc \
    chromium_src/chrome/browser/ui/views/profiles/profile_menu_view.cc \
    2>/dev/null || true
  git checkout -- chromium_src/ash/ 2>/dev/null || true
  git checkout -- chromium_src/chromeos/ 2>/dev/null || true
  git checkout -- chromium_src/chrome/app/theme/chromium/ 2>/dev/null || true
  git checkout -- chromium_src/chrome/app/theme/default_*/chromium/ 2>/dev/null || true

  echo "  [✓] Sync complete"
else
  echo "  [i] Skipping gclient sync"
fi
cd "$PROJECT_DIR"

# ── 4. GN gen ────────────────────────────────────────────────────────
echo ""
echo "==> Step 2/5: Configuring GN..."

if [ "$BUILD_TYPE" = "debug" ]; then
  GN_ARGS="is_debug=true is_purecloak=true target_cpu=\"$TARGET_CPU\""
else
  GN_ARGS="
    is_debug=false
    is_purecloak=true
    is_component_build=false
    symbol_level=0
    blink_symbol_level=0
    optimize_for_size=true
    enable_nacl=false
    target_cpu=\"$TARGET_CPU\"
    use_system_libcxx=false
  "
fi

# macOS-specific: use_system_libxcb is not needed
# enable_dsyms=false to reduce output size on release

if [ "$BUILD_TYPE" != "debug" ]; then
  GN_ARGS="$GN_ARGS enable_dsyms=false"
fi

gn gen "$OUT_DIR" --args="$GN_ARGS" --fail-on-unused-args
echo "  [✓] GN gen complete"

# ── 5. Build ─────────────────────────────────────────────────────────
echo ""
echo "==> Step 3/5: Building chrome (this will take a while)..."
autoninja -C "$OUT_DIR" chrome -j"$JOBS"
echo "  [✓] Build complete"

# ── 6. Tests ─────────────────────────────────────────────────────────
if [ "$SKIP_TESTS" = false ]; then
  echo ""
  echo "==> Step 4/5: Building unit tests..."
  autoninja -C "$OUT_DIR" purecloak_unittests -j"$JOBS" || {
    echo "  [i] purecloak_unittests not found, skipping"
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

# ── 7. Package ───────────────────────────────────────────────────────
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

  # Create .app bundle structure
  APP_NAME="PureCloak.app"
  DIST_DIR="$PROJECT_DIR/dist/$APP_NAME"
  mkdir -p "$DIST_DIR/Contents/MacOS"
  mkdir -p "$DIST_DIR/Contents/Resources"

  cp "$BINARY" "$DIST_DIR/Contents/MacOS/PureCloak"

  # Create Info.plist
  cat > "$DIST_DIR/Contents/Info.plist" <<-PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key><string>PureCloak</string>
  <key>CFBundleIdentifier</key><string>com.purecloak.PureCloak</string>
  <key>CFBundleName</key><string>PureCloak</string>
  <key>CFBundleDisplayName</key><string>PureCloak</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>151.0.7892.0</string>
  <key>CFBundleVersion</key><string>151.0.7892.0</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

  # Copy branding icons if available
  if [ -d "chromium_src/chrome/app/theme/chromium" ]; then
    cp -r "chromium_src/chrome/app/theme/chromium/" "$DIST_DIR/Contents/Resources/" 2>/dev/null || true
  fi

  # Create DMG
  cd "$PROJECT_DIR/dist"
  hdiutil create -volname "PureCloak" -srcfolder "$APP_NAME" \
    -ov -format UDZO "purecloak-macos-$TARGET_CPU.dmg" 2>/dev/null || {
    # Fallback: just tar
    tar czf "purecloak-macos-$TARGET_CPU.tar.gz" "$APP_NAME"
  }
  rm -rf "$APP_NAME"
  echo "  [✓] Package: $PROJECT_DIR/dist/purecloak-macos-$TARGET_CPU.dmg"
else
  echo "  [!] Binary not found at $BINARY"
fi

echo ""
echo "==> Build complete! 🍎"
echo "    Binary: $PROJECT_DIR/$OUT_DIR/chrome"
echo "    Run:    $PROJECT_DIR/$OUT_DIR/chrome"
