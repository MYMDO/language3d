# NPC foundation (Roadmap Phase 4, v0.38.0)

Status: implemented. Foundation only — no dialogue, quests, inventory,
language profiles, AI, schedules, persistence or multiplayer. Not wired to
the renderer or `Game`, by design.

## Relationship: Entity ↔ NPCAgent

```text
EntityPool      identity: id + kind + alive/enabled
   │ 1:1 (by generational Entity id, owner-validated)
TransformPool   Transform3 storage (shared with all future systems)
NPCPool         NPCAgent record: archetype, state, order, target, speed…
```

`NPCAgent` (not `NPC`: `core.h` already owns the legacy 2D single-NPC game
state; the two converge when gameplay migrates to entities) stores the
Entity id but never dereferences the `EntityPool` — the same decoupled
pattern as `TransformPool`/`ColliderPool`. Owner discipline: detach the
NPC before destroying its entity; the NPC generation guard blocks
slot-reuse aliasing of NPC ids themselves.

## Lifecycle

`spawn(entity, archetype)` → `IDLE` → `command_move(target, speed)` →
`MOVING` → (`ARRIVED` | `BLOCKED`) → `command_stop()` → `IDLE` →
`destroy()`. Stale ids rejected everywhere; double-destroy safe;
uninitialized pools refuse spawns.

## Movement model (kinematic, deterministic)

`update(tpool, id, dt_ms, blocked)` advances the entity's transform toward
the target in per-axis clamped steps: exact arrival, fixed-point only,
`speed·dt/1000` per tick. `blocked(from, to)` is an owner-provided
predicate consulted per axis step (a later phase binds the real
`walk_move`/collision here). Z is preserved; height integration arrives
with the physics phase. No-transform-yet waits in `IDLE` without failing.

## Memory layout (measured)

| Item | Bytes |
|---|---|
| `NPCAgent` record | 28 |
| `NPCPool<16>` slot | 32 |
| **`NPCPool<16>` total** | **528 B SRAM** |

Pools are caller-owned aggregates (static storage on RP2040).
`npc_selfcheck()` compile-proves the unit on every toolchain.

## Future extension points (reserved, not implemented)

Dialogue trees, quest-giver flags, inventory handles, schedules, language
profiles — each becomes its own fixed pool keyed by NPC id, mirroring how
`NPCAgent` keys by Entity id today. Schedules are explicitly Roadmap
Phase 5, after foundation movement/interaction is proven.

## Footprint delta (Etap 33)

RP2040 firmware `.bin`: **60436 → 60436 (+0 B)** — `npc.cpp` is linked
but unreferenced symbols are dropped by `--gc-sections`. Desktop: no
measurable change. Tests: 21 → 22.
