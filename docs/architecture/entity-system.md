# Entity system (Roadmap Phase 3, v0.37.0)

Status: implemented. Foundation only — no NPC, AI, Quest, Inventory or
Dialogue on this milestone. Not wired to the renderer or `Game` yet, by
design: the API and tests stabilize first.

## Anti-monolith rule

`Entity` carries **identity only** (id + kind + lifecycle). Everything else
lives in separate fixed pools. This file, not a future refactor, is where
the monster-struct is prevented:

```text
EntityPool<N>      id + kind + alive/enabled + free list
TransformPool<N>   Transform3 per slot (existing type, never duplicated)
ColliderPool<N>    feet-anchored box + solid flag
(NPCPool / ItemPool / … arrive in later phases with the same pattern)
```

## ID semantics

`u16 id = (generation << 8) | slot_index`, capacity `1 ≤ N ≤ 256`
(`static_assert`). Helpers: `entity_slot_index()`,
`entity_generation()`, `entity_make_id()`.

- First life of a slot uses generation **1** (never 0).
- `destroy()` bumps the generation, skipping 0 on wrap, and pushes the
  slot to the free-list head (most-recent-free reuse → deterministic).
- Stale ids are rejected: unknown slot, `!alive`, or generation mismatch.
- Double-destroy is safe (returns `false`).
- `L3D_ENTITY_INVALID (0xFFFF)` = allocation failure (pool full).
- Uninitialized pools refuse `spawn()` — safe by default.

## Generation semantics (two-level honesty)

- Component pools store a **generation copy** at `attach()` time.
- `get()` rejects ids whose generation differs → **slot-reuse aliasing
  is impossible**.
- Owner discipline (documented, tested): the owner detaches an entity's
  components **before** `destroy()`. The generation copy guards reuse,
  not owner discipline. The future World registry will own this pairing.

## Lifecycle

`spawn(kind)` → alive + enabled → `set_enabled(id, false)` disables
without destroying → `destroy(id)`. Iteration (`for_each_active`) visits
**active** entities in **slot-index order** — fully deterministic,
independent of spawn/destroy history.

## Transform / Collider

- `Transform3` is reused as-is (`pos` = feet point, `yaw`).
- `Collider{hx, hy, h, solid}`: box = `{pos-(hx,hy,0), pos+(hx,hy,h)}`.
- `collider_world_box()` bridges entities to World3 volumes: an entity's
  box is directly testable against `TriggerVolume`/`Region3`
  (`entity_test` proves NPC-standing-in-trigger addressing). No rendering
  integration — `RenderablePool` arrives only when a renderer consumes it.

## Maximum count & memory cost (measured, x86-64 / ARM identical layout)

Recommended world budget: `L3D_ENTITY_BUDGET = 64`.

| Pool⟨64⟩ | Bytes | Per slot |
|---|---|---|
| `EntityPool` | 400 | 6 |
| `TransformPool` | 1152 | 18 (`Transform3` 16 + gen + flag) |
| `ColliderPool` | 1152 | 18 (`Collider` 16 + gen + flag) |
| **Total** | **2704 B SRAM** | |

Pools are caller-owned aggregates (static storage on RP2040).
`entity_selfcheck()` compile-proves the unit on every toolchain.

## Footprint delta (Etap 33)

RP2040 firmware `.bin`: **60436 → 60436 (+0 B)** — `entity.cpp` is linked
but unreferenced symbols are dropped by `--gc-sections`. Desktop: no
measurable change. Tests: 20 → 21.

## Next (Roadmap Phase 4 — NPC)

`NPCPool` + schedules on top of `EntityPool`/`TransformPool`; dialogue,
items, quests and language pools follow the same fixed-pool pattern.
