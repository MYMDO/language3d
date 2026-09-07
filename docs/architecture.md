# Language3D Architecture (as of v0.35.0)

## 1. Existing architecture

```text
engine/            platform-independent core (verified: zero SDL/Pico/OS includes)
├── core.h         Fx 16.16, Config (WIDTH/HEIGHT per profile), SimulationClock
├── game.h/.cpp    Game: player, NPC, doors, sprites, modes (Explore/Dialogue/Quiz/…)
├── renderer.h/.cpp SoftwareRaycaster: DDA raycaster → caller-owned indexed8 FB
├── assets.h/.cpp  zero-copy L3DP v1/v2 AssetPackView + builtin + generated pack
├── api.h/.cpp     C ABI: init/update/render, framebuffer + input structs
├── world.h        DoorSystem<N> runtime overlay over immutable packed map
├── entities.h     EntityPool / SpriteBatch (fixed-size, allocation-free)
├── world_grid.h   WorldGridView: zero-copy 16×16 chunk view
├── transform3.h / world3.h (.cpp)  Phase-1 3D world MODEL (standalone, tested)
└── math/capabilities/resource_cache

platform/sdl2/main.cpp        desktop loop: SDL events → latch → sim ticks →
                              render → palette→RGB565 texture → letterbox present
platform/rp2040/src/main.cpp  firmware loop: GPIO buttons → engine → ST7789 SPI
platform/rp2040/src/display.cpp  PROVEN ST7789 driver (do not refactor casually)
platform/esp32s3/             null/reference backend (not a release target)
```

Build: root CMake (`LANGUAGE3D_PLATFORM=host|rp2040`), presets, CTest
(19 tests), `make` legacy wrapper. CI: Linux / Windows-MSVC-static /
RP2040 + releases. Profiles: `L3D_PC_PROFILE` (1920×1080) vs default/
`L3D_RP2040_PROFILE` (240×240) via `Config`.

## 2. What is already extensible

- GameCore has **no platform dependencies** (verified by grep audit).
- Assets are zero-copy views — content can grow without engine copies.
- Fixed 60 Hz integer-ms simulation is platform-neutral already.
- Resolution is centralized in `Config`; no stray literals.
- `world3` models X/Y/Z geometry, stairs, bridges, triggers, regions.

## 3. Gap analysis (verified findings)

| # | Gap | Evidence |
|---|-----|----------|
| G1 | **No input abstraction.** Two independent implementations: SDL scancodes (`platform/sdl2/main.cpp`) vs GPIO levels (`platform/rp2040/src/main.cpp`). Game-specific key mapping duplicated. | grep `Scancode` / `gpio_get` |
| G2 | **No presentation abstraction.** SDL texture-upload path and ST7789 SPI path are hole separately, both hand-rolled per backend. | `main.cpp` ×2 |
| G3 | **No time abstraction.** `SDL_GetTicks()` vs `to_ms_since_boot()` inline in loops. | both mains |
| G4 | **Renderer is 2D-bound.** DDA over `(mapX,mapY)`, flat floor, full-height walls, camera table per `Config::WIDTH`. `world3` heights are invisible to it. | `renderer.cpp` DDA, `make_camera_x_table` |
| G5 | **World state is 2D-bound.** `Game::solid`, doors, sprites use X/Y cells; `Player` has no Z; `InputState` has no vertical. | `game.h`, `core.h` |
| G6 | **Renderer↔world coupling is 2D-shaped.** `SoftwareRaycaster` holds `AssetPackView` + `DoorSystem<8>*` directly with silent `builtin_assets()` fallback. | `renderer.h:47-65` |
| G7 | **No entity system.** `EntityPool`/`SpriteBatch` cover sprites only; no generic Entity+Transform+components. | `entities.h` |
| G8 | **No save, no content pipeline, no NPC/dialogue/quest/item/language/save subsystems.** Only build-time `embed_assets.py`. | grep save/json: no runtime |
| G9 | `world3` model is **not wired** to `Game`/renderer yet (standalone + tested). | v0.35.0 scope |

Non-gaps (do not "fix"): no platform includes in `engine/`; no hardcoded
resolutions outside `Config`; camera-alignment invariant tested (v24).

## 4. Target architecture (evolution, not rewrite)

```text
Language3D
├── Core (engine/): Game, World3, Entity, Math, Simulation — no platform APIs
├── Renderer: software indexed8 backends (raycaster now, 3D-capable later)
├── Platform API (platform/api/): init/time/events/keys/framebuffer/audio*
│   ├── SDL2 backend      ├── RP2040 native backend      └── null backend
├── Gameplay systems (phased): NPC, Dialogue, Quest, Inventory, Language, Save
├── Content (data-driven, validated; Phase 11+)
└── Platform mains (thin loops: events → sim ticks → render → present)
```

`*audio: interface reserved, null-only until a real need appears (no overengineering).`

## 5. Phase map (brief Etap 30)

- v0.35.0 — brief Phase 2-partial (world3 model) + Phase 3-partial (`walk_move`).
- **v0.36.0 — brief Phase 1: thin Platform API (this milestone).**
- Next: brief Phase 4 (entity system) → wire world3 into Game → renderer
  adaptation → NPC/dialogue/items/quests/language/save → content validation
  → vertical slice. One phase per commit(s), all targets green each time.

## 6. Invariants for every phase

1. `engine/` keeps zero platform includes (CI-greppable).
2. RP2040 `display.cpp` untouched without hardware-verified reason.
3. No gameplay change smuggled into architecture phases.
4. Linux + Windows + RP2040 CI green; firmware size tracked per phase.
5. No heavyweight ECS / scripting runtime / network / LLM until real need.
