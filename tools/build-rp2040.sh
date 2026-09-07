#!/usr/bin/env bash
# Build the production RP2040/ST7789 game firmware from a clean directory.
# Requires: ARM GCC toolchain + Raspberry Pi Pico SDK.
# Usage: PICO_SDK_PATH=$HOME/pico-sdk ./tools/build-rp2040.sh
set -euo pipefail
if [ -z "${PICO_SDK_PATH:-}" ]; then
  echo "error: PICO_SDK_PATH is not set (e.g. export PICO_SDK_PATH=\$HOME/pico-sdk)" >&2
  exit 2
fi
# Always configure from scratch so a stale CMakeCache can never hide problems.
rm -rf build-rp2040
cmake --preset rp2040-release
cmake --build --preset rp2040-release
echo "---- firmware artifacts ----"
ls -la build-rp2040/language3d_rp2040.uf2 \
       build-rp2040/language3d_rp2040.bin \
       build-rp2040/language3d_rp2040.hex \
       build-rp2040/language3d_rp2040.elf
