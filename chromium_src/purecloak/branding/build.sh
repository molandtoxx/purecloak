#!/bin/bash
# Build PureCloak and ensure purecloak binary is available
set -e
cd "$(dirname "$0")/../../.."
autoninja -C out/purecloak chrome
./chromium_src/purecloak/branding/post_build.sh out/purecloak
