# Player state (Roadmap Phase 10, v0.44.0)

Status: implemented. Minimal deterministic aggregation — not a
"GameState Everything". No quests, language profile, save files or
multiplayer.

## Reference, not copy

```text
EntityPool ── generational id ──→ PlayerState.entity
                                       │
Inventory<8/16> ←── non-owning ptr ────┘ (nullable, any capacity)
Transform3      ←── via Entity id ── (never stored here)
```

The player IS an entity: no Transform3, no identity duplication. The
inventory stays a separate ownership domain — `PlayerState` cannot outlive
or duplicate it, only associate. Entity liveness follows the codebase-wide
owner discipline (record keeps the id; owner validates/rebinds).

## Contents (justified minimum)

`xp/level` (placeholder curve `1 + xp/1000`, cap 99), `reputation`,
32 world `flags` (future dialogue/quest gating), 8 `counters` (generic
progression tallies). No score field (xp covers it); no quest/language
members (later pools reference the player id, as NPC does the entity id).

## Runtime vs persistent (structural from day one)

- `PlayerState` — runtime record: bindings + working set.
- `PlayerPersistent` — 32 B versioned plain data (`version=1` gate):
  entity, xp, level, reputation, flags, counters.
- `snapshot()/restore()` convert; pointers never persist (orchestrator
  rebinds inventory after load); render/interaction caches are not
  represented here at all and must never be saved.

This is what future dialogue effects (`give_item`) and quest conditions
(`has_item`) will operate through — the destination exists before the
writers.

## Memory & footprint

`PlayerState` = **48 B** (any inventory capacity — pointer, not storage);
`PlayerPersistent` = **32 B**. Firmware `.bin`: **60436 → 60436 (+0 B)**.
Desktop: no measurable change. Tests: 27 → 28.

## Next (Phase 11 — Quests)

Quest state machines with `TALK/REACH/COLLECT/GIVE/USE` objectives over
exactly these primitives.
