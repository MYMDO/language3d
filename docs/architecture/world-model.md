# True 3D world model (Roadmap Phase 2, shipped v0.35.0)

Status: implemented. Additive only: the 2.5D raycaster, `Game`,
the C ABI, all platform layers and all pre-existing tests are untouched.

## Principle: World 3D ≠ Rendering 3D

Roadmap Phase 2 introduces a **three-dimensional world model** while the renderer
stays 2.5D. Geometry, locomotion and volumes are real and tested; pixels
catch up in a later phase. This keeps every existing target (including the
RP2040 firmware) working at each step.

## Convention

```text
X = east / west
Y = north / south
Z = height (up)
```

- Positions are feet points in 16.16 fixed point (`Fx`), same as `Game`.
- Yaw reuses the engine-wide 65536-units-per-turn convention
  (`TrigLut::sin16/cos16`). No pitch/roll yet — yaw-only locomotion.
- Eye height = feet + 1.6; body = radius 0.2, height 1.7 (`World3Limits`).

## Files

- `engine/transform3.h` — `Vec3`, `Transform3`, `yaw_forward()`, `AABB3`,
  `aabb_contains/overlaps()`. Header-only, no heap, no exceptions.
- `engine/world3.h` / `engine/world3.cpp`:
  - `HeightField` — zero-copy view over caller-owned bytes
    (1 byte/cell/layer, 1/8-unit quantum, flash-friendly).
    `floorQ == nullptr` means flat 0; `ceilQ == nullptr` means
    floor + 2.0. An **empty field means "no world"** and denies movement.
  - `walk_move()` — axis-separated slide with step-up (≤ 0.45) and
    headroom checks. Solid cells collide only on real vertical overlap,
    so **bridges and tunnels work**: a deck at +3.0 is walkable-under.
    Feet track the floor up and down stairs (no free fall yet — physics
    is a later phase, deliberately).
  - `TriggerVolume` / `Region3` with `trigger_at()` / `region_at()`.
    `id` / `contentTag` are **opaque u16**: quest ids, vocabulary-pack
    ids, district ids… Meaning is assigned by later content phases,
    never by the world model.
  - `world3_selfcheck()` — deterministic no-I/O check compiled into
    every target (including RP2040) so each toolchain parse-proves the
    unit. Unused → dropped by `--gc-sections` (firmware `.bin` is
    byte-identical in size with and without this phase).

## Memory budget

Per authored cell: 1 byte floor (+1 byte optional ceiling), resident in
flash via zero-copy views — e.g. a 76×76 level costs ~5.6 KiB of flash,
0 bytes of SRAM beyond the caller's `Vec3`. No `World3` state struct
exists yet; that arrives with the entity system (Roadmap Phase 3).

## Tested behavior (`tests/world3_test.cpp`, 19-test CTest suite)

Flat walk, wall slide (X denied / Y passes), stair climb with feet+eye
tracking, too-tall step denied, low ceiling denied, walk under bridge,
trigger enter/exit + exclusive max bound, region tag lookup,
`world3_selfcheck()`.

## Explicitly NOT in this milestone

Renderer changes, `Game`/ABI changes, entities, NPCs, items, quests,
language data, JSON content, save system, jumping/falling physics,
multi-level rendering. Next: **Roadmap Phase 3 — entity system**
(`Entity` + `Transform3` + type tag + fixed pool, still no heap).
