# Language3D

Portable first-person 2.5D engine with an allocation-free MCU path and a
native desktop profile sharing the same game core.

- **Engine** (`engine/`): platform-independent fixed-point simulation,
  software raycaster, indexed-8 framebuffer, zero-copy `.l3dp` assets.
- **Desktop** (`platform/sdl2/`): native-resolution SDL2 frontend
  (1920×1080 on PC).
- **RP2040** (`platform/rp2040/`): production game firmware for Raspberry Pi
  Pico + ST7789 240×240 over SPI (see [`docs/platforms/rp2040.md`](docs/platforms/rp2040.md)).
- **ESP32-S3** (`platform/esp32s3/`): null/reference backend (not a release target yet).

## Download

Ready-to-use binaries are attached to every versioned
**[GitHub Release](../../releases)** (`vMAJOR.MINOR.PATCH` tags):

- `language3d-<version>-linux-x86_64.tar.gz` — Linux game
- `language3d-<version>-windows-x86_64.zip` — Windows game
- `language3d-<version>-rp2040.uf2` — Pico firmware (drag-and-drop flashing)
- `language3d-<version>-rp2040.bin` / `.hex` — alternative firmware formats
- `language3d-<version>-rp2040-debug.zip` — `.elf` + `.map` for debugging

## Build

One command per platform (see [`docs/platforms/`](docs/platforms/) for details):

```bash
./tools/build-linux.sh    # Linux SDL2 game  -> build-linux/language3d
./tools/build-rp2040.sh   # RP2040 firmware  -> build-rp2040/*.uf2 (.bin/.hex/.elf)
```

Windows (native MSVC + vcpkg SDL2, no MSYS2 needed):

```powershell
cmake -S . -B build-windows -DLANGUAGE3D_PLATFORM=host `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build-windows --config Release
```

Or use the [CMake presets](CMakePresets.json) directly:
`linux-debug`, `linux-release`, `windows-debug`, `windows-release`, `rp2040-release`.

The single source of truth for the version is the [`VERSION`](VERSION) file,
surfaced as `PROJECT_VERSION` by the root [`CMakeLists.txt`](CMakeLists.txt)
via `-DLANGUAGE3D_PLATFORM=host|rp2040`.

## Controls (desktop)

Click the window to capture the mouse (`Esc` releases it).

| Key | Action |
| --- | ------ |
| `W`/`S`, arrows `↑`/`↓` | move forward / back |
| `A`/`D`, `Q`/`C` | strafe left / right |
| arrows `←`/`→`, mouse | turn |
| `E` / `L` / `P` / `H` | interact / language / progress / help |
| `1`–`3` | quiz answer |
| `F3` | camera debug probe |
| `F11` | fullscreen toggle |

RP2040 buttons and ST7789 wiring: [`docs/platforms/rp2040.md`](docs/platforms/rp2040.md).

## Tests

```bash
./tools/test.sh          # full host suite via CTest (34 tests)
make test-all            # legacy Make wrapper (core subset)
```

The suite covers engine core, renderer, assets, generated pack integrity,
input, world/doors, camera alignment, map connectivity, the 3D
world model (Roadmap Phase 2) and the Platform API contract (null backend). RP2040 display
and input require real hardware: CI verifies firmware compilation, linking
and artifact presence, and documents hardware testing as a manual step.

## Platform direction

Language3D evolves from tech MVP toward a 3D language-learning game
platform: explore → see → understand → talk → act → solve → reward →
remember. The engine grows in additive roadmap phases (see the phase table
in [`docs/architecture.md`](docs/architecture.md)), each keeping all
targets green. Game/platform separation is enforced by the thin Platform
API (`platform/api/l3d_platform.h`, backends for SDL2 / RP2040 / null);
the 3D world model lives in
[`docs/architecture/world-model.md`](docs/architecture/world-model.md).

## Releases

Maintainers cut a release by pushing a version tag:

```bash
echo 0.35.0 > VERSION
git commit -am "Release 0.35.0" && git push
git tag v0.35.0 && git push origin v0.35.0
```

The `release` workflow then builds all platforms, packages the archives and
publishes the GitHub Release automatically. Regular pushes/PRs only run CI
checks and upload intermediate artifacts.

## Project history

Development notes for previous versions live in
[`docs/CHANGELOG.md`](docs/CHANGELOG.md).
