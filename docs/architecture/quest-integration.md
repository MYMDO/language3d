# Quest integration (Roadmap Phase 11B, v0.46.0)

Status: implemented in three layers — rewards, dialogue wiring, one fully
playable quest. No language scoring, no shops, no multiplayer.

## 11B-1 — Rewards with atomic transactions

`QuestDef` gains up to 4 rewards (`XP / ITEM / FLAG / COUNTER`, still no
language-mastery kind — rewards stay in game-state space). `quest_claim`
now takes the item bank + player and commits **atomically**:

```text
validate ALL (item space via can_fit dry-run, flag/counter ranges)
   ↓ fail → CLAIM_BLOCKED, quest stays COMPLETED, nothing applied
   ↓ ok   → commit everything → CLAIMED
```

Never a half-state (`+XP` but no item while `CLAIMED`). `Inventory::can_fit`
(a local-copy dry run, additive API) makes the check possible without
mutating. Content syntax: `reward XP:100`, `reward ITEM:2:1`,
`reward FLAG:5`, `reward COUNTER:1:1`.

## 11B-2 — Dialogue cond/effect wiring (no cycles)

Dialogue nodes outgrew single reserved bytes:

```text
cond:   NONE | HAS_ITEM:i:c | FLAG_SET:b | COUNTER_GE:i:t | LEVEL_GE:l
effect: NONE | GIVE_ITEM:i:c | SET_FLAG:b | ADD_COUNTER:i:n | ADD_XP:n
        | START_QUEST:q
```

Param layouts mirror `QuestCondition`/`QuestReward` on purpose — one mental
model. Crucially, the dialogue engine still evaluates nothing: all meaning
lives in `engine/dialogue_quest.h`, the first cross-subsystem unit, so
neither engine includes the other:

```text
dq_begin    open + TALK fan-out to every ACTIVE quest + entry effect
dq_choose   cond-gated advance + entered-node effect (leftover reported)
dq_cond_met / dq_apply_effect   the two primitives
```

GIVE_ITEM with a full inventory reports PARTIAL + leftover instead of
losing the item; START_QUEST forwards `quest_start` and reports its event.

## 11B-3 — Playable: Getting to the Station

Anna (dialogue 1, START_QUEST effect) → quest ACTIVE → walk to the station
district (REACH) → clerk (dialogue 2, GIVE_ITEM ticket effect) → COLLECT →
GIVE → COMPLETED → claim (+100 XP, flag 5). Proven by
`tests/playable_quest_test.cpp`, a scripted game loop over ALL pools
(entities → dispatch → interaction → dialogue → quest → inventory →
player) with generated content banks. Anna also walks her schedule alongside.

## Memory & footprint

Rewards add ~24 B per quest def (flash). `QuestLog` unchanged.
Firmware `.bin`: **60436 → 60436 (+0 B)**. Desktop: no measurable change.
Tests: 29 → 31.

## Next (Phase 12 — Language system)

Vocabulary/grammar/mastery/adaptive dialogue on top of the metadata
already riding every node and item. The loop above runs unmodified —
only its difficulty becomes level-aware.
