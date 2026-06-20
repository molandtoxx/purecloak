#!/bin/bash
# Apply PureCloak anti-detection patches to the Chromium source tree.
# Run this after every `gclient sync` to reapply PureCloak modifications.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="$REPO_ROOT/chromium_src"

if [ ! -d "$CHROMIUM_SRC" ]; then
    echo "ERROR: Chromium source tree not found at $CHROMIUM_SRC"
    echo "Run this script from the PureCloak repository root."
    exit 1
fi

echo "[PureCloak] Applying anti-detection patches..."
cd "$CHROMIUM_SRC"

PATCH_DIR="$REPO_ROOT/patches"
APPLIED=0
FAILED=0

for patch in "$PATCH_DIR"/*.patch; do
    [ -f "$patch" ] || continue
    echo "  Applying: $(basename "$patch")..."
    if patch -p1 --forward --silent < "$patch" 2>/dev/null; then
        echo "    ✓ Success"
        APPLIED=$((APPLIED + 1))
    else
        # Check if already applied (reverse check)
        if patch -p1 --reverse --dry-run --silent < "$patch" 2>/dev/null; then
            echo "    - Already applied, skipping"
        else
            echo "    ✗ Failed to apply (conflict)"
            FAILED=$((FAILED + 1))
        fi
    fi
done

echo ""
echo "[PureCloak] Done: $APPLIED applied, $FAILED failed"
exit $FAILED
