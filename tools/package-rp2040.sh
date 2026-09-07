#!/usr/bin/env bash
# Package RP2040 firmware artifacts for a release.
# Usage: ./tools/package-rp2040.sh [output-dir]
# Produces versioned .uf2/.bin/.hex copies plus a debug bundle (.elf + .map).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
OUT="${1:-$ROOT/dist}"
BUILD="$ROOT/build-rp2040"
for f in language3d_rp2040.uf2 language3d_rp2040.bin language3d_rp2040.hex language3d_rp2040.elf; do
  if [ ! -f "$BUILD/$f" ]; then
    echo "error: $BUILD/$f not found — run ./tools/build-rp2040.sh first" >&2
    exit 2
  fi
done
mkdir -p "$OUT"
cp "$BUILD/language3d_rp2040.uf2" "$OUT/language3d-$VERSION-rp2040.uf2"
cp "$BUILD/language3d_rp2040.bin" "$OUT/language3d-$VERSION-rp2040.bin"
cp "$BUILD/language3d_rp2040.hex" "$OUT/language3d-$VERSION-rp2040.hex"
STAGE="$OUT/stage-rp2040-debug"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/language3d_rp2040.elf" "$BUILD/language3d_rp2040.elf.map" "$STAGE/" 2>/dev/null || cp "$BUILD/language3d_rp2040.elf" "$STAGE/"
cp "$ROOT/docs/platforms/rp2040.md" "$STAGE/FLASHING.md" 2>/dev/null || true
(cd "$STAGE" && zip -qr "$OUT/language3d-$VERSION-rp2040-debug.zip" .)
echo "OK: $OUT/language3d-$VERSION-rp2040.uf2 (.bin/.hex/-debug.zip too)"
