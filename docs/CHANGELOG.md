## v24.3 PC native-1080p optimization
- PC renderer remains native 1920×1080; there is no internal 960×540 upscale.
- Desktop framebuffer storage is heap-backed to prevent stack exhaustion at 1920×1080.
- SDL streaming texture uses RGB565, matching the display transport format.
- Palette→RGB565 conversion uses a 256-entry LUT and writes directly into the locked SDL texture, eliminating the previous ARGB8888 staging buffer.
- PC profile reserves one engine framebuffer; SDL owns its presentation texture separately.
- Embedded profiles remain independent and retain their own buffer-count policy.
- Existing camera/input alignment and 240×240 embedded assumptions are unchanged.

# Language 3D — Pareto v24.3 PC native 1080p

## v23 input and test hardening
- Platform-independent `ControlTuning` with fixed-point movement/strafe and turn rates.
- Optional mouse X delta in the C ABI; mouse handling remains a desktop concern.
- Q/C strafe is added without changing A/D turning semantics.
- Regression tests no longer use `assert()`: tests use always-on `L3D_REQUIRE`, so Release builds actually validate conditions.
- Added `v23_input_test` for strafe and mouse-turn behavior.

## v19 input robustness fix

Desktop SDL adapter now reads physical `SDL_Scancode` values for movement, so WASD works regardless of active keyboard layout. Arrow keys are accepted as an alternative. One-shot actions use scancodes as well.



Portable first-person 2.5D engine with an allocation-free MCU path.

## v11 focus

- No virtual dispatch in the frame hot path.
- Software raycaster remains the reference renderer for low-end systems.
- Fixed-point simulation and renderer math remain platform-neutral.
- Caller-owned indexed 8-bit framebuffer.
- Double-buffer display contract for RP2350B.
- Compact zero-copy `.l3dp` assets.
- Fixed-size `ResourceCache<N>` for immutable resource views; no heap, no copies.
- Compact `EntityPool<8>` / `SpriteBatch` for allocation-free billboard scenes.
- Depth-tested sprite renderer with stable distance ordering.
- RP2350 platform storage guard aligned with the actual engine ABI.
- RP2350B/ESP32-S3 platform layers remain outside the engine core.

## Architecture

```text
Game + portable math
        |
   SoftwareRaycaster
        |
 indexed8 framebuffer
        |
 platform display/HAL
   /              \
RP2350B           ESP32-S3
DMA/PIO           LCD/GDMA
```

Desktop/Web may use the same core while selecting a higher-level presentation backend.

## Resource model

The `.l3dp` pack is the canonical runtime resource container. Resource views point
into the packed blob. `ResourceCache<N>` only stores pointers and sizes, so assets
can remain in Flash/PSRAM/SD-backed memory without a runtime copy.

## Tests

`make core-test`

`make renderer-test`

`make asset-test`

`make display-test`

`make v10-test`

`make v11-test`

The current container does not guarantee SDL2/Pico SDK availability; platform
smoke tests therefore use their null/reference backends where appropriate.


## v12 world/door layer
- Material `0` remains empty, `1..14` are regular materials, `15` is a dynamic door marker.
- Packed map data stays immutable; runtime door state is kept in a fixed `DoorSystem<N>`.
- A door blocks collision and ray traversal while `open < 255`; when fully open it becomes transparent to movement and raycasts.
- No heap allocation is required for runtime door state.
- `world-test` covers packed-map discovery, interaction, animation, collision and full-open traversal.

## v13 Pareto hardening
- Fixed multi-texture addressing: texture slot N now selects the Nth 64x64 payload.
- Material 15 (dynamic door) maps to texture slot 1 when available.
- Sprite sorting no longer mutates the authoritative EntityPool; it uses a tiny fixed index array.
- Framebuffer clear uses row-wise memset; wall ray setup advances per column rather than recomputing camera position divisions.
- Vertical wall texture sampling uses an incremental fixed-point accumulator instead of a division per pixel.
- Door animation now carries a millisecond accumulator so small frame deltas do not quantize to zero motion.
- Added v13 regression test for texture selection and entity-order preservation.


## v13 performance/correctness hardening
- Fixed multi-texture addressing in `AssetPackView::texture8()`: texture slot N now points at the Nth 64x64 payload.
- Separated world-material selection from sprite texture selection; L3DP v1 world materials currently use wall texture 0 until a compact material table is introduced.
- Sprite rendering no longer reorders the authoritative entity pool; sorting uses a fixed local index array.
- Framebuffer clear is row-wise `memset`, reducing per-pixel loop overhead.
- Camera rays advance by a precomputed per-column step instead of recomputing camera-position multiplication/division for every column.
- Wall texture V coordinates use an incremental fixed-point accumulator, removing a division from the wall inner pixel loop.
- Door animation uses a millisecond accumulator so sub-frame deltas do not quantize movement away.
- `SpriteEntity` remains compact and has an explicit size assertion.
- Added regression coverage for texture-slot selection, sprite order preservation, and the v13 render path.

## Verification
All host-side regression targets pass under `-std=c++17 -O2 -Wall -Wextra -Wpedantic`:

```text
core-test       OK
renderer-test   OK
asset-test      OK
display-test    OK
v10-test        OK
v11-test        OK
world-test      OK
v13-test        OK
asset-pack      OK
python syntax   OK
```

The RP2350 CMake target remains SDK-dependent; the reference host environment does not include the Pico SDK, so firmware compilation is not claimed here.


## v14 profiling and memory hardening
- Engine-owned per-column depth storage replaces the process-global fallback buffer.
- Render statistics are collected inside the renderer and exposed through the C ABI for profiling/regression testing.
- Statistics cover ray count, DDA steps, wall pixels, floor pixels and visible sprite work.
- The statistics structure is intentionally small and remains inside the fixed engine state budget.
- Added v14 telemetry regression coverage.
- Profiling is part of the engineering loop: optimization decisions should be based on measured DDA steps and pixel counts rather than guesses.

## v15 reciprocal optimization
- Added a 257-entry compile-time Q16 reciprocal table (1 KiB) for the hot DDA/projection path.
- Linear interpolation removes per-ray floating-point math and replaces the previous reciprocal integer division with table lookup + multiply/interpolation.
- Wall-height projection now consumes the same reciprocal primitive.
- Wall texture V-step initialization uses the reciprocal primitive as well.
- Added `v15-test`, including correctness assertions and a repeatable 120-frame host benchmark.
- The public C ABI and framebuffer format remain unchanged.

### Optimization policy
Measured host benchmark output is recorded by the test executable rather than treated as a portable performance claim. Final RP2350B/ESP32-S3 performance must be measured on target silicon with the actual display transport enabled.

## v16 hot-path optimization
- Removed repeated `AssetPackView::valid()` parsing from the per-pixel texture path by resolving validated asset data once and using explicit unchecked accessors in the hot renderer path.
- Removed repeated map-header validation from DDA traversal by caching map pointer and dimensions for the current frame.
- Sprite texture sampling is now incremental in both axes; the inner pixel loop contains no coordinate division.
- Camera direction for sprite rendering is computed once per frame rather than once per sprite.
- Kept the public ABI, indexed8 framebuffer contract, and zero-copy `.l3dp` resource model unchanged.
- Added a 240-frame host benchmark and v16 regression target.

### v16 principle
Optimize measured hot paths before adding abstraction or features. Asset header parsing, map validation and per-pixel coordinate division were higher-leverage targets than additional renderer features.


## v16 measured hot-path hardening
- Runtime reciprocal indexing now uses a power-of-two step (`>> 8` / `& 0xFF`) plus multiply/shift interpolation; the previous runtime `/ STEP` operations are removed from `inv_abs()`.
- The reciprocal table is 513 entries (about 2 KiB), covering the normalized 0..2.0 Q16 range used by the current camera FOV.
- The renderer resolves map pointer/dimensions once per frame and uses direct texture payload addressing in wall/sprite inner loops.
- Sprite texture coordinates use incremental accumulators; no per-pixel coordinate division remains in the sprite inner loop.
- Added the v16 240-frame benchmark and retained all previous regression tests.

### Performance discipline
The v16 host numbers are intentionally treated as local measurements only. Scheduler noise and compiler/CPU differences can exceed small optimization deltas, so target silicon measurements remain authoritative for RP2350B and ESP32-S3.

## v17 architecture hardening
- Added `WorldGridView`, a zero-copy 16x16 chunk view over the immutable L3DP grid. It is intentionally much smaller than a general voxel/chunk engine and exists to provide scalable spatial organization without sacrificing the MCU path.
- Added a tiny `RendererCapabilities` POD contract for capability-based backend selection. No virtual dispatch or heavyweight scene/driver objects are introduced.
- Renderer reuses the cached `WorldGridView` for map traversal; fallback construction is only used when the renderer is invoked directly without an engine-owned world view.
- Kept material/texture/resource ABI unchanged; this is an additive architectural layer rather than an incompatible format rewrite.
- Added `v17-test` covering chunk coordinates, world caching integration and renderer capabilities.


## v18 cross-platform architecture hardening
- Added a deterministic 60 Hz `SimulationClock`. Since 1000 ms is not divisible by 60, it uses an exact 16/17 ms Bresenham-style tick schedule whose total is exactly 1000 ms per 60 simulation ticks. Platform frame cadence is decoupled from gameplay tick cadence, with bounded catch-up to avoid a simulation spiral after stalls.
- The C ABI continues to expose variable-frame `l3d_engine_update_ms()`, while the engine internally converts it to fixed simulation ticks.
- Added a top-level CMake build for the portable core, optional SDL2 frontend, and CTest integration. This becomes the primary path for desktop/CI builds; Make remains available as a lightweight developer wrapper.
- Added `v18_test` covering the 60 Hz rational tick schedule, catch-up bounds, ABI initialization and rendering.
- Architecture remains intentionally layered: core has no OS dependency; platform frontends/adapters depend downward on the core. This follows the common platform-abstraction pattern used across game engines while avoiding a heavyweight runtime framework.

## Design basis
The project deliberately combines high-value ideas found across the historical FPS/game-engine landscape: Wolfenstein-style low-memory software rendering; chunk/spatial organization and resource versioning ideas from larger engines; and swappable renderer/platform boundaries. The goal is not to clone any particular engine, but to preserve the small 80/20 subset that materially improves portability, determinism, memory efficiency and extensibility.

## v20 input correctness

Desktop simulation uses `SimulationClock` and passes integer millisecond tick durations to `Game::update()`. The SDL frontend never passes a fractional `float` duration to the integer-millisecond API. Movement/turning therefore remains active at normal 60 Hz frame cadence. SDL physical scancodes are used for keyboard layout-independent WASD/arrow controls.

## v20 desktop-input fix

The SDL frontend uses an event-driven held-key latch (KEYDOWN/KEYUP) with physical scancode mapping, plus `SDL_GetKeyboardState()` as a recovery path. Simulation is driven by `SimulationClock`; `Game::update()` receives exact integer-millisecond tick durations. One-shot actions are latched until consumed by a simulation tick.

## v20 verification note

The v19 desktop adapter computed `float dt = frame_ms / 1000.f` and passed it to `Game::update(..., u32 dt_ms)`. Normal frame values such as `0.016f` therefore converted to integer `0`, disabling movement and turning. v20 removes that conversion path: the desktop adapter advances `SimulationClock` and passes exact integer tick durations (`16`/`17` ms) directly to `Game::update()`.

## v21 visual correctness pass

The camera now uses a compile-time exact pixel-center Q16.16 camera-X table rather than the earlier approximate 411-unit column increment. This keeps the runtime ray setup division-free while removing accumulated horizontal sampling skew. A regression test checks that the demo's head-on planar wall remains a constant-height projection across the screen and that turning changes the rendered frame.

## v21 visual correctness

v21 fixes a renderer-critical reciprocal-range bug: the previous lookup saturated distances above 2.0 world units, which could make distant walls appear vastly too large. The reciprocal is now normalized to [1,2) with a 257-entry Q16.16 LUT and exponent adjustment. Camera sampling also uses exact pixel-center Q16.16 coordinates. A visual regression test verifies a frontal wall remains nearly constant in projected height and is not allowed to consume most of the frame at the initial 4.5-unit distance.

## Clean checkout / build

This source archive intentionally contains no generated `build/` directory. Configure from the `work/` directory:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## v24 display-profile regression fix

- Root cause: `make_camera_x_table()` in `engine/renderer.cpp` normalized the
  per-column camera-space X coordinate against a hardcoded `320` literal
  instead of `Config::WIDTH`. Invisible while `WIDTH == 320`; at `WIDTH == 240`
  the "center" screen column no longer corresponded to a straight-ahead ray,
  which is what `v21_visual_test`'s `center_top` assertion caught.
- Fix: both occurrences of the `320` literal now derive from `Config::WIDTH`.
- `tests/display_test.cpp` also had a hardcoded `320, 240` fixture, unrelated
  to the renderer bug but caught by the same resolution change; parameterized
  against `Config::WIDTH/HEIGHT`.
- `engine/core.h`: `Config::WIDTH` set to `240` — this build's actual target
  is the RP2040 + 1.3" ST7789 240x240 profile, not the earlier 320x240
  desktop-only default.
- Swept `engine/`, `platform/`, `tests/` for other literal `320`/`240`/`76800`
  occurrences after the fix; none remain tied to resolution (the only other
  `240` is `v16_test.cpp`'s unrelated 240-frame benchmark loop count).

### New architectural invariant
No component in GameCore, RendererCore, or platform-independent code may
contain a hardcoded value that depends on a specific display, resolution,
aspect ratio, or transport format. Any such parameter must come from the
Hardware/Display Profile (`Config` today; a richer per-profile struct as
more display targets are added).

### Regression baseline
```text
Target display: 240x240 (RP2040 / ST7789 profile)
Tests:           16/16
```
This state is the new baseline. Subsequent changes should diff against it
rather than the original 320x240-default checkout, to avoid re-accumulating
undocumented local fixes.


## v0.24 PC-Linux profile

The desktop frontend is built with `L3D_PC_PROFILE`. It renders internally at 1920×1080 in the same indexed-8 software framebuffer architecture and presents a native nearest-neighbour image in a 1920×1080 SDL2 window. The game core, fixed-point simulation, asset API, and renderer API remain shared with the embedded profile.

Build on Fedora with: `sudo dnf install gcc-c++ make SDL2-devel pkgconf-pkg-config`, then `make run`. `F11` toggles fullscreen desktop mode.

This desktop profile is a host test target, not a claim about RP2040 performance. Embedded builds retain the 240×240 profile.

## v0.24.1 desktop viewport fix

- Removed SDL logical-size stretching from the PC frontend.
- The 960x540 render surface is now fitted explicitly into the actual SDL drawable area while preserving its 16:9 aspect ratio.
- Letterboxing is centered, so the renderer center/reticle remains the true center of the game viewport under the 1920x1080 window and during resize/fullscreen.
- Added the documented F11 fullscreen toggle.
- GameCore/RendererCore remain unchanged; this is a desktop presentation-layer fix.

## v0.24.2 — Memory telemetry breakdown

The PC/Linux build now reports a component-level reservation breakdown, separate single-buffer and total double-buffer indexed8 framebuffer sizes, and Linux VmHWM alongside RSS. The telemetry also prints a 240×240 embedded memory model for RP2040/RP2350 comparison; the embedded model reuses the current deterministic upper-bound reservations and is not a live MCU measurement.


## v0.24.3 — camera/forward alignment regression guard

- Fixed `make_camera_x_table()` in `engine/renderer.cpp`: the pixel-coordinate term was accidentally scaled by 2, causing the middle column to use `camera_x ~= +1` instead of `~0`. This made the view direction differ substantially from the simulation forward vector.
- Kept all movement/FOV constants unchanged; the fix corrects camera geometry rather than compensating movement.
- Added an explicit camera-alignment invariant via `v24-camera-alignment-test`: the center camera ray must be parallel to the simulation forward vector across multiple headings, and a point 8 world units directly ahead must project to the reticle center.
- Added `F3` in the PC frontend to show a temporary projected-forward probe for visual diagnosis.
- Fixed `v21_visual_test` to use `Config::WIDTH` for its horizontal sample range and updated its center-wall expectation for the corrected forward-facing geometry.


### PC first-person camera
Click the game window to capture the mouse. Relative horizontal mouse motion controls yaw without requiring Enter. `W/S` move forward/back, `A/D` strafe, and Left/Right arrows provide keyboard-only turning. `Esc` releases the mouse capture; press `Esc` again to use the normal game escape action.


## v0.24.3 mouse sensitivity

Reduced PC mouse-look sensitivity from 256 to 64 turn units per relative mouse pixel (4× slower). Camera alignment logic is unchanged.


## v0.24.3 native 1080p PC profile
The PC profile now renders natively at 1920×1080 instead of rendering at 960×540 and upscaling. Embedded profiles remain independent.

## v0.25.0 — exact vertical wall sampling

The wall-column sampler no longer quantizes projected wall height before computing the vertical texture step. The renderer keeps projected wall height and top/bottom edges in Q16.16 and initializes `texV` from the actual pixel center, preventing per-column texture-phase jumps that appeared as sawtooth horizontal wall lines at native high resolutions.

The fix is renderer-local and keeps the existing framebuffer, palette, transport, camera-alignment invariant, and fixed-point architecture unchanged.

## v0.26.0 — procedural wall material

The built-in wall visual no longer depends on `wall0.raw` for the wall-column path. The
wall surface is generated procedurally from the continuous hit coordinates and sampled
vertical coordinate, preserving the existing fixed-point renderer and camera alignment
invariants while removing the 64×64 wall texture as a visual dependency. The existing
asset API remains available for other content.

## v0.27.0 wall seam rendering fix

Horizontal wall seams are now rendered as projected geometric boundaries from continuous Q16.16 wall height rather than as a one-texel nearest-neighbor texture row. Panel fill no longer alternates on `panel_y`, preventing sub-pixel phase differences from turning horizontal boundaries into repeating triangular/sawtooth artifacts at high resolution.

All host regression tests pass, including the camera-alignment test.


## v0.28.0 — build-generated map content

The PC build now embeds `assets/map.txt` and `assets/textures.raw` into `build/generated_assets.cpp` before compilation. The SDL2 runtime binds this immutable generated pack instead of silently falling back to the hardcoded demo map. Editing `assets/map.txt` is therefore reflected in the next `make`/`make run`; GameCore and RendererCore remain unchanged.

The L3DP v1 header parser follows the documented field layout: `mapW=data[5]`, `mapH=data[6]`, `textureCount=data[7]`. The included default map is a 76x76 labyrinth, about 10.03x the cell area of the old 24x24 demo, with the player start at (4,4) and NPC at (18,18) connected by an open route.

## v0.28.1 — generated asset size API fix

Fixed the PC SDL2 startup diagnostic to use `AssetPackView::size` (a data member) rather than calling it as a function. This is a build-only fix; generated map loading and the game/renderer architecture are unchanged.

## v0.31.0
- L3DP v2 supports u16 map dimensions and u32 payload sizes for 1024x1024+ worlds.
- PC benchmark map: 1024x1024.


## v0.31.0 — 5120x5120 map stress benchmark
This benchmark scales the map dimensions 5x from 1024x1024 to 5120x5120 (25x the area). Runtime renderer and world representation are unchanged. The L3DP v2 u16 map dimensions permit this size.

## v0.31.2 — gameplay-reachable 5120x5120 maze
Replaced the v0.31.1 stress map with a deterministic connected serpentine labyrinth. The gameplay start (4,4) and NPC target (18,18) are guaranteed to be in the same open component. Added `tools/validate_map_path.py` and `make map-path-test` as a regression guard. No renderer, GameCore or runtime asset-format changes.
