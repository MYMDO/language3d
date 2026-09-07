#!/usr/bin/env bash
# Package a Linux release archive from an existing build-linux directory.
# Usage: ./tools/package-linux.sh [output-dir]
# Produces: language3d-<version>-linux-x86_64.tar.gz
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
OUT="${1:-$ROOT/dist}"
cmake --build "$ROOT/build-linux" --preset linux-release 2>/dev/null || cmake --build "$ROOT/build-linux"
STAGE="$OUT/stage-linux"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cmake --install "$ROOT/build-linux" --prefix "$STAGE/language3d-$VERSION-linux-x86_64"
cp "$ROOT/docs/platforms/linux.md" "$STAGE/language3d-$VERSION-linux-x86_64/README-platform.md" 2>/dev/null || true
tar -czf "$OUT/language3d-$VERSION-linux-x86_64.tar.gz" -C "$STAGE" "language3d-$VERSION-linux-x86_64"
echo "OK: $OUT/language3d-$VERSION-linux-x86_64.tar.gz"
