#!/usr/bin/env bash
#
# build-android.sh — Cross-compile PureCloak for Android ARM64 from Linux
#
# Builds chrome_public_apk target for Android. Requires a Linux build machine
# with all Chromium build dependencies installed.
#
# Usage: bash build-android.sh [options]
#   --debug          Build debug (default: release)
#   --out DIR        Output directory (default: out/android)
#   --skip-sync      Skip gclient sync (use existing checkout)
#   --arm64          Build for ARM64 (default)
#   --x86            Build for Android x86 (emulator)
#   --jobs N         Parallel build jobs (default: nproc)
#
# Prerequisites (one-time):
#   sudo ./chromium_src/build/install-build-deps.sh --android
#   # Also needs: openjdk-17-jdk (for APK signing)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Config ──────────────────────────────────────────────────────────
BUILD_TYPE="release"
OUT_DIR="out/android"
SKIP_SYNC=false
TARGET_CPU="arm64"
JOBS="$(nproc 2>/dev/null || echo 4)"

# ── Parse args ─────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)       BUILD_TYPE="debug"; shift ;;
    --out)         OUT_DIR="$2"; shift 2 ;;
    --skip-sync)   SKIP_SYNC=true; shift ;;
    --arm64)       TARGET_CPU="arm64"; shift ;;
    --x86)         TARGET_CPU="x86"; shift ;;
    --jobs)        JOBS="$2"; shift 2 ;;
    --help|-h)
      sed -n '2,13p' "$0"
      exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

cd "$PROJECT_DIR"
echo "==> PureCloak Android Build (cross-compile from Linux)"
echo "    Project:  $PROJECT_DIR"
echo "    Type:     $BUILD_TYPE"
echo "    Target:   Android $TARGET_CPU"
echo "    Out:      $OUT_DIR"
echo "    Jobs:     $JOBS"

# ── 1. Check prerequisites ──────────────────────────────────────────
echo ""
echo "==> Checking prerequisites..."

for cmd in python3 git ninja java; do
  if command -v "$cmd" &>/dev/null; then
    echo "  [✓] $cmd"
  else
    echo "  [!] $cmd not found (may come from depot_tools)"
  fi
done

# Check for Android SDK/NDK (will be downloaded by gclient)
# We just need the build deps installed
if [ ! -f "/usr/include/elf.h" ] && [ ! -f "/usr/include/linux/elf.h" ]; then
  echo "  [!] Linux build deps may not be installed"
  echo "  Run: sudo ./chromium_src/build/install-build-deps.sh --android"
fi

# ── 2. depot_tools ──────────────────────────────────────────────────
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

# ── 3. gclient sync (Android) ──────────────────────────────────────
if [ "$SKIP_SYNC" = false ]; then
  echo ""
  echo "==> Step 1/5: Syncing Chromium source with Android deps..."

  # Android needs target_os = ['android'] in .gclient
  # Create a separate .gclient file for Android builds
  cat > "$PROJECT_DIR/.gclient.android" <<-EOF
solutions = [
  {
    "name": "chromium_src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": True,
    "custom_deps": {},
    "custom_vars": {},
  },
]
target_os = ['android']
EOF

  cd "$PROJECT_DIR"

  # Use the Android-specific gclient file
  export GCLIENT_FILE="$PROJECT_DIR/.gclient.android"

  gclient sync --shallow --nohooks -j"$JOBS"
  gclient runhooks

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
  unset GCLIENT_FILE
else
  echo "  [i] Skipping gclient sync"
fi
cd "$PROJECT_DIR"

# ── 4. GN gen (Android) ─────────────────────────────────────────────
echo ""
echo "==> Step 2/5: Configuring GN for Android $TARGET_CPU..."

if [ "$BUILD_TYPE" = "debug" ]; then
  GN_ARGS="
    is_debug=true
    is_purecloak=true
    target_os=\"android\"
    target_cpu=\"$TARGET_CPU\"
  "
else
  GN_ARGS="
    is_debug=false
    is_purecloak=true
    is_component_build=false
    symbol_level=0
    blink_symbol_level=0
    optimize_for_size=true
    enable_nacl=false
    target_os=\"android\"
    target_cpu=\"$TARGET_CPU\"
    dcheck_always_on=false
    enable_iterator_debugging=false
  "
fi

gn gen "$OUT_DIR" --args="$GN_ARGS" --fail-on-unused-args
echo "  [✓] GN gen complete"

# ── 5. Build ─────────────────────────────────────────────────────────
echo ""
echo "==> Step 3/5: Building chrome_public_apk (this will take a while)..."
autoninja -C "$OUT_DIR" chrome_public_apk -j"$JOBS"
echo "  [✓] Build complete"

# ── 6. Locate APK ───────────────────────────────────────────────────
echo ""
echo "==> Step 4/5: Locating APK..."

APK_DIR="$OUT_DIR/apks"
if [ -d "$APK_DIR" ]; then
  APK_FILES=$(find "$APK_DIR" -name "*.apk" 2>/dev/null)
  if [ -n "$APK_FILES" ]; then
    echo "  APK(s) found:"
    echo "$APK_FILES" | while read -r apk; do
      echo "    - $apk ($(du -h "$apk" | cut -f1))"
    done
  else
    echo "  [!] No APK files found in $APK_DIR"
  fi
else
  echo "  [!] APK directory not found at $APK_DIR"
  echo "  Checking $OUT_DIR for APKs..."
  find "$OUT_DIR" -name "*.apk" 2>/dev/null | head -5
fi

# ── 7. Package ───────────────────────────────────────────────────────
echo ""
echo "==> Step 5/5: Packaging..."

# Find and package APKs
APKS=$(find "$OUT_DIR" -name "*.apk" 2>/dev/null)
if [ -n "$APKS" ]; then
  DIST_DIR="$PROJECT_DIR/dist/purecloak-android-$TARGET_CPU"
  mkdir -p "$DIST_DIR"
  while IFS= read -r apk; do
    cp "$apk" "$DIST_DIR/"
  done <<< "$APKS"

  cd "$PROJECT_DIR/dist"
  tar czf "purecloak-android-$TARGET_CPU.tar.gz" "purecloak-android-$TARGET_CPU"
  rm -rf "purecloak-android-$TARGET_CPU"
  echo "  [✓] Package: $PROJECT_DIR/dist/purecloak-android-$TARGET_CPU.tar.gz"
else
  echo "  [!] No APKs found to package"
fi

echo ""
echo "==> Build complete! 🤖"
echo "    APK: $PROJECT_DIR/$OUT_DIR/apks/"
echo "    Install: adb install $PROJECT_DIR/$OUT_DIR/apks/ChromePublicTest.apk"
