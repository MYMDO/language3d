# NPC schedules (Roadmap Phase 5, v0.39.0)

Status: implemented. Schedules only — no movement binding, no AI, no
dialogue triggers yet. (Binding "be at SHOP at 09:00" to actual walking is
the interaction milestone that follows.)

## Data-oriented, location-ID based

A schedule never stores coordinates. Each entry is:

```text
[start_min, end_min) → locationTag + behavior
```

`locationTag` lives in the same opaque content namespace as
`Region3::contentTag`: the world/content layer defines what HOME/SHOP/
CAFE/PARK mean and where they are. End ≤ start expresses an overnight
wrap (21:00→08:00). First covering entry in authoring order wins —
deterministic priority for overlaps.

The same mechanism is designed to later serve quest objectives, spawn
points, shops, zones, dialogue context and language-topic context: all of
them resolve a locationTag through the world layer instead of hardcoding
geometry.

## Time

`GameClock{day, minute, ms_acc, ms_per_minute}` — deterministic day/minute
time advanced with integer millisecond deltas (default scale: 1 game-minute
per real second; 0 disables). Plain data, tick-for-tick reproducible,
trivially saveable. Default 08:00, day 0.

## Ownership (unchanged domains)

`SchedulePool` keys schedules by **full generational NPC id** (equality
match — a reused NPC slot never inherits the previous schedule).
`EntityPool` owns existence, `NPCPool` owns NPC state, `SchedulePool`
owns timetables. No cross-pool dereferences anywhere.

## Memory layout (measured)

| Item | Bytes |
|---|---|
| `ScheduleEntry` | 8 |
| `GameClock` | 16 |
| `SchedulePool<16>` slot | 80 (8 entries × 8 B + id + count) |
| **`SchedulePool<16>` total** | **1288 B SRAM** |

Caller-owned aggregates; `schedule_selfcheck()` compile-proves the unit
on every toolchain.

## Footprint delta (Etap 33)

RP2040 firmware `.bin`: **60436 → 60436 (+0 B)**. Desktop: no measurable
change. Tests: 22 → 23.

## Next

Bind schedules to movement (NPC walks to its current locationTag's region
via the world layer), then player approach → interaction → dialogue.
