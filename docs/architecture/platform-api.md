# Platform API (Roadmap Phase 1, shipped v0.36.0)

Contract: `platform/api/l3d_platform.h` — pure C, fixed-size types, no heap
on the caller, single-threaded. Covers lifecycle, monotonic time, input
events + key snapshot, mouse mode, display size/fullscreen, indexed8→RGB565
presentation. Audio is a reserved future extension (no interface until a
real need — see `docs/architecture.md` invariant 5).

## Backends (one linked per binary)

| Backend | File | Notes |
|---|---|---|
| SDL2 | `platform/sdl2/platform_sdl2.cpp` | Owns window/renderer/streaming texture, LUT upload, letterbox. Maps SDL scancodes → `l3d_key_t`. |
| RP2040 native | `platform/rp2040/src/platform_rp2040.cpp` | Production ST7789 path + GPIO buttons reused as-is; edges synthesized in `poll()`. Same pins, same probe, same stats. |
| null | `platform/null/` | Headless, stdlib-only. Scripted event injection via `platform_null.h` (tests/tools only, never shipped). |

Dependency direction is strict: `platform → engine`, never reverse.
`engine/` still has zero platform includes (audited).

## Loop contract (both game mains)

```text
pf_init → assets → while(run): ticks → drain poll() → latch → sim ticks →
render → present → stats → pf_shutdown
```

One-shot actions persist until consumed by a simulation tick; the key
snapshot (`l3d_pf_key_down`) is the recovery path for dropped edges;
`FOCUS_LOST` clears held state. Behavior of both loops is preserved 1:1
from v0.35.0 — only the address of input/present/time code moved.

## Cost (Etap 33 baseline delta, RP2040 firmware)

- Flash `.bin`: 59860 → 60436 (+576 B, backend code).
- RAM `.bss`: 145900 → 146068 (+168 B; 16-deep event queue + levels).
- Desktop: no measurable change (same SDL calls, smoke-tested headless).
- Tests: 19 → 20 (`platform_test`: lifecycle, caps, FIFO, snapshot,
  present validation).

## Adding a future backend (SDL3 / raylib / …)

1. Implement the ~12 functions in `l3d_platform.h` against the new SDK.
2. Link it instead of the current backend in `CMakeLists.txt`.
3. No `engine/` or game-loop logic changes required.
