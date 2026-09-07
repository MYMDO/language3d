# Quest foundation (Roadmap Phase 11A, v0.45.0)

Status: foundation (11A) + integration (11B) implemented. Definitions,
state machine, objectives, conditions, atomic rewards, dialogue wiring,
one playable quest. No shops, no language scoring yet.

## No god object (explicit principle)

The quest engine sees ONLY objective / condition / state. Include direction
is one-way (`quest.h -> player.h -> item.h`); dialogue/NPC/schedule/world
units are never included. Tags are opaque content ids; orchestration of
Dialogue x Quest x Inventory belongs to a future Game/Simulation layer:

```text
                    Game/Simulation (future)
                         │
        ┌────────┬───────┼────────┬────────┐
        ▼        ▼       ▼        ▼        ▼
      Player    NPC    Dialogue  Quest   Inventory
        │        │       │        │        │
        └────────┴───────┴────────┴────────┘
                         │
                    shared IDs/state
```

## State machine

`NOT_STARTED → ACTIVE → COMPLETED → CLAIMED`. `start` checks prerequisites
(referenced quests at minimum states) and rejects double-start; `report`
advances the current sequential objective (out-of-order events are
`IGNORED`, never errors); `claim` acknowledges completion (payload in 11B).

## Objectives & conditions (standard gameplay only, no UNDERSTAND/SPEAK yet)

`TALK(tag) / REACH(tag) / COLLECT(item×n) / GIVE(item×n→npc) /
USE(item) / INSPECT(tag)`. COLLECT accumulates quota (`PROGRESS` events).
GIVE checks possession without consuming (transfer effects are 11B).
Conditions: `ALWAYS / HAS_ITEM / FLAG_SET / COUNTER_GE / LEVEL_GE`,
evaluated against `PlayerState` (+ inventory association) at report time;
unmet conditions report `CONDITION_UNMET` instead of silently passing.

## Data-driven content

`content/quests/*.quest` → `tools/build_quests.py` → static tables,
validated at build time and by the `validate-content` CI job (same
pipeline as dialogue/items). First quest: Getting to the Station
(TALK Anna → REACH station → COLLECT ticket → GIVE clerk).

## Memory & footprint

`QuestDef` 216 B flash (mostly the 8-objective table), `QuestRuntime`
6 B, `QuestLog<16>` **104 B SRAM**. Firmware `.bin`:
**60436 → 60436 (+0 B)**; quest tables linked into host tests only.
Tests: 28 → 29.

## Next (Phase 12 — Language system)

Vocabulary/grammar/mastery/adaptive dialogue on the metadata already
riding every node and item (see quest-integration.md for the wired loop).
