# Schedule-driven movement (Roadmap Phase 6A, v0.40.0)

Status: implemented. NPCs now physically live by the world clock: schedule
flip → walk to the new location → arrival. No dialogue, interaction, AI or
quests yet.

## The living-world loop

```text
GameClock.minute
      ↓
SchedulePool.lookup(npc) → ScheduleEntry{locationTag}
      ↓
Region3 with contentTag == locationTag → center (X/Y)
      ↓
NPCPool.command_move + update (existing kinematic model)
      ↓
ARRIVED (held until the schedule changes)
```

All four subsystems (`GameClock`, `SchedulePool`, `NPCPool`,
`TransformPool`, `Region3`) stay decoupled. The ONLY meeting point is
`engine/npc_dispatch.h`, split into independently testable steps:

- `npc_active_entry()` — schedule resolution;
- `resolve_region_target()` — tag → region center (exact raw halving, Z
  taken from current feet so the target is always reachable and height is
  never lost; first matching region wins);
- movement — unchanged `NPCPool::update()`;
- arrival — unchanged NPC state machine.

`npc_dispatch_step()` orchestrates one NPC per call: no schedule → IDLE,
unknown tag → IDLE (never a crash), unchanged arrival → hold ARRIVED
(without re-entering `update()`, which would fold back to IDLE), changed
target → re-command at cruise speed. Dispatch is stateless across calls
except through the NPC record itself (target comparison).

## Framerate independence

Dispatch consumes only `dt_ms`; the game loop feeds it from
`SimulationClock` ticks like everything else. Proven by test: 10×100 ms
vs 100×10 ms over the same 12 s reach the identical arrived state
(Fx arrival clamping makes rounding converge exactly).

## Memory & footprint

Dispatch adds **no storage**: header-only glue + `npc_dispatch_selfcheck()`.
RP2040 firmware `.bin`: **60436 → 60436 (+0 B)**. Desktop: no measurable
change. Tests: 23 → 24.

## Example day (tested)

```text
07:55 Anna HOME (arrived) → 08:00 SHOP (moving) → walk → SHOP (arrived)
→ 13:00 CAFE … → 23:59 CAFE → 00:59 HOME (moving, overnight wrap)
```

## Next (Phase 6B — interaction)

Player approach → proximity → interact, on top of NPCs that are now
actually findable at schedule-correct places. Then dialogue.
