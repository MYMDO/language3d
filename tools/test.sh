#!/usr/bin/env bash
# Build (if needed) and run the full host test suite via CTest.
# Usage: ./tools/test.sh [debug|release]
set -euo pipefail
MODE="${1:-release}"
case "$MODE" in
  debug)   PRESET=linux-debug ;;
  release) PRESET=linux-release ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac
cmake --preset "$PRESET" >/dev/null
cmake --build --preset "$PRESET"
ctest --preset "$PRESET"
