# NPC interaction (Roadmap Phase 7, v0.41.0)

Status: implemented. Proximity + explicit action + session state. No
dialogue content yet — the session's NPC id is the handoff point the
dialogue phase will consume.

## Chain (first playable behavior)

```text
World3 + Entity + NPC + Schedule + Movement + Player + Input
   ↓
proximity → target → E edge → STARTED … ENDED
```

## Selection (world coordinates only, never rendering)

1. Inside radius — X/Y plane, squared fixed-point distance, no sqrt.
2. Facing hemisphere gate — `dot(forward, dir) > 0`, exact integer math.
3. Lowest **Entity** id — independent of NPC slot/iteration order (tested
   with deliberately inverted orders).

Deliberate scope call: facing is a gate, not a continuous dot ranking —
exact normalized-dot ordering needs a square root and buys nothing for
"approach and press E". Documented here so a future aiming phase can
revisit it consciously.

## Session (`InteractSession`, caller-owned)

`interact_try(target, now_ms)` → `STARTED | ALREADY | COOLDOWN |
NO_TARGET`; `interact_end(now_ms)` → `ENDED | NONE`. One session at a
time; 500 ms default debounce; time comes from the caller's clock
(platform ticks on device), never from rendering. E-key wiring point:
caller forwards the `L3D_KEY_E` edge from the Platform API.

## Memory & footprint

Session state is **12 bytes** (id + timestamps + flags), owned by the
caller — the module itself adds no storage. RP2040 firmware `.bin`:
**60436 → 60436 (+0 B)**. Desktop: no measurable change. Tests: 24 → 25.

## Next (Phase 8 — Dialogue)

Dialogue trees consume `session.npc` + NPC schedule/location context;
quest/item/language layers follow the same fixed-pool pattern.
