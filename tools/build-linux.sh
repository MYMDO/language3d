#!/usr/bin/env bash
# Configure + build the Linux host version (SDL2 game + test suite).
# Usage: ./tools/build-linux.sh [debug|release]
set -euo pipefail
MODE="${1:-release}"
case "$MODE" in
  debug)   PRESET=linux-debug ;;
  release) PRESET=linux-release ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
echo "OK: preset $PRESET built. Run ./tools/test.sh $MODE to execute the tests."
