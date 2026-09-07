# Save system (Roadmap Phase 13, v0.48.0)

Status: implemented. Local versioned persistence for clock, player,
inventory, quests and language profile. No cloud, no accounts, no files
in the engine (buffers in, buffers out).

## One format for host and RP2040

Line-based decimal text (`L3DSAVE 1`, `CLOCK …`, `PLAYER …`, `COUNTERS …`,
`INVENTORY …` + `SLOT` lines, `QUESTS …` + `QUEST` lines, `LANGUAGE …` +
`WORD` lines, `END`), LF-terminated. Rationale: deterministic (fixed field
order — identical bytes for identical state, tested with memcmp),
portable (ASCII), endian-safe (no binary integers), independent of C++
memory layout (no struct dumps), tiny hand-rolled codec (no locale, no
heap). A populated mid-quest save measures ~170 bytes.

## Infrastructure, not a subsystem

`engine/save.h` reads/writes snapshots and owns nothing: single grammar
driver (`save_parse`) with two sinks — `SaveCheckSink` (structure only)
and `SaveApplySink` (fills live state, validates content ids). No
duplicated grammar, no virtual dispatch, no heap.

## Atomicity by construction

Serialize into the caller's staging buffer → `save_validate()` → commit
(write-temp+rename on the platform side). Truncated staging never
validates, so a torn write can never look like a save. `save_read`
assumes validated input and re-checks everything while applying (callers
treat load as all-or-nothing; partial application on unvalidated input is
documented, not a supported path).

## Versions and migration

`SAVE_VERSION = 1`. The version line dispatches: `1` → parse, higher →
`NEEDS_MIGRATION` (the single hook future migrations attach to — none
pre-built), garbage → `BAD_VERSION`. Rejection taxonomy: `MALFORMED`
(grammar/range), `TRUNCATED` (EOF mid-grammar, cursor-precise),
`OVERFLOW` (staging/capacity too small), `UNKNOWN_ID` (content drift).

## What persists (and what never does)

Persist: clock (day/minute/acc/scale), player progression + flags +
counters + entity id (opaque), non-empty inventory slots in order,
tracked quests (def/state/objective/progress), pair + level + per-word
counters. Never: pointers, live references, platform/renderer objects,
caches, sessions. Content banks stay out of the save (progress references
them); unknown item/quest/word ids on load reject the save strictly
instead of silently dropping progress. Entity ids persist opaquely —
the owner rebinds liveness after load.

## Memory & footprint

Codec + snapshots add no state (staging buffer is the caller's).
Firmware `.bin`: **60436 → 60436 (+0 B)**. Desktop: no measurable change.
Tests: 33 → 34.

## Next

Persist NPC/schedule runtime state if the vertical slice needs it, then
adaptive language gameplay (assessment-driven CORRECT/INCORRECT) over the
persisted counters.
