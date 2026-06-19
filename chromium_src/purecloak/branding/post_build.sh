#!/bin/bash
# Post-build script: ensure purecloak binary exists alongside chrome
# Called after autoninja completes
# Usage: post_build.sh [output_dir]

OUT_DIR="${1:-out/purecloak}"

if [ -f "$OUT_DIR/chrome" ] && [ ! -f "$OUT_DIR/purecloak" ]; then
  ln -f "$OUT_DIR/chrome" "$OUT_DIR/purecloak"
  echo "Created $OUT_DIR/purecloak (hardlink to chrome)"
elif [ -f "$OUT_DIR/chrome" ] && [ -f "$OUT_DIR/purecloak" ]; then
  echo "$OUT_DIR/purecloak already exists"
else
  echo "No chrome binary found at $OUT_DIR/chrome"
  exit 1
fi
