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
├── transform3.h / world3.h (.cpp)  3D world MODEL (standalone, tested)
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
├── Content (data-driven, validated; later phase)
└── Platform mains (thin loops: events → sim ticks → render → present)
```

`*audio: interface reserved, null-only until a real need appears (no overengineering).`

## 5. Roadmap phases vs versions (single notation)

Roadmap phases are **logical milestones**; versions are **chronological
releases**. The two sequences are tracked separately and never mixed:

| Roadmap phase | Scope | Status |
|---|---|---|
| Phase 0 — Audit | gap analysis, architecture plan | ✅ done (`docs/architecture.md`) |
| Phase 1 — Platform abstraction | thin Platform API + backends | ✅ done, shipped **v0.36.0** |
| Phase 2 — True 3D world | X/Y/Z model, heights, volumes | ✅ done, shipped **v0.35.0** |
| Phase 3 — Entity system | Entity/Transform/Collider pools | ✅ done, shipped **v0.37.0** |
| Phase 4 — NPC | NPC foundation (no dialogue/schedules yet) | ✅ done, shipped **v0.38.0** |
| Phase 5 — NPC schedule | location-ID timetables + game clock | ✅ done, shipped **v0.39.0** |
| Phase 6 — Schedule-driven movement (6A) | NPCs walk their schedules | ✅ done, shipped **v0.40.0** |
| Phase 7 — NPC interaction (6B) | proximity/target/session, pre-dialogue | ✅ done, shipped **v0.41.0** |
| Phase 8 — Dialogue | data-driven trees + inert language metadata | ✅ done, shipped **v0.42.0** |
| Phase 9 — Items / Inventory | defs/stacks/inventory, no trade yet | ✅ done, shipped **v0.43.0** |
| Phase 10 — Player state | entity ref + progression + runtime/persistent split | ✅ done, shipped **v0.44.0** |
| Phase 11 — Quests (A) | defs/state/objectives/conditions, no rewards yet | ✅ done, shipped **v0.45.0** |
| Phase 11B — Quest integration | atomic rewards + dialogue wiring + playable quest | ✅ done, shipped **v0.46.0** |
| Phase 12 — Language system | vocabulary/grammar/mastery/adaptive | next |
| Phase 12 — Language system | vocabulary/grammar/mastery/adaptive | next |
| Phase 13 — Save / Progression | persistent versioned state | planned |
| Phase 14 — Vertical slice | playable educational scenario | planned |

Note: phase numbers are logical, not chronological — Phase 2 (world)
landed as v0.35.0 *before* Phase 1 (platform) as v0.36.0. That is
intentional: each phase ships when its API and tests are stable.

## 6. Invariants for every phase

1. `engine/` keeps zero platform includes (CI-greppable).
2. RP2040 `display.cpp` untouched without hardware-verified reason.
3. No gameplay change smuggled into architecture phases.
4. Linux + Windows + RP2040 CI green; firmware size tracked per phase.
5. No heavyweight ECS / scripting runtime / network / LLM until real need.
