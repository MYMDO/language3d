# Item / inventory foundation (Roadmap Phase 9, v0.43.0)

Status: implemented. Definitions, stacks and inventory ops only —
`add/remove/has/count/clear` (+ carried `weight`). No use/equip/consume/
trade, no shops, no quest hooks yet.

## Data-driven definitions (no items in C++)

```text
content/items/*.item   strict text format (see content/items/README.md)
        ↓  tools/build_items.py (parse + validate + emit)
build/generated_items.cpp   static ItemDef tables (flash-resident)
        ↓  zero-copy ItemBank view
```

Invalid content (duplicate ids, unknown category, `max != 1` on
non-stackables, over-long names) fails the generator, which fails every
build plus the `validate-content` CI job. A runtime validator
(`item_validate*`, same rules) guards programmatic banks. Starter content:
apple, station ticket, rusty key, phrasebook.

## No dependency cycles (explicit principle from this phase on)

`engine/item.h` includes nothing but core headers. Items never reference
dialogue/quest/NPC/schedule units; subsystems meet only through shared
opaque ids under future Game/Simulation orchestration:

```text
Game/Simulation orchestration
        │
   ┌────┼────┬────┬──────┐
   ▼    ▼    ▼    ▼      ▼
 NPC  Dialogue Item Quest Language
        │      │    │
        └──────┴────┴──→ shared ids/state
```

## Semantic hooks without duplication

`ItemDef` carries `category` + opaque `vocab[]` into the SHARED content
vocabulary namespace already used by dialogue nodes (e.g. 103=station).
Items duplicate no language data — the future vocabulary subsystem
resolves `Item → vocab_id → entry`. `weight` is carried per piece
(abstract units); enforcement (encumbrance) is a later gameplay decision.

## Inventory semantics (deterministic)

- Fixed `N` slots (`Inventory<8>` = 40 B, `<16>` = 72 B SRAM).
- `add` tops up existing stacks in slot order, then opens new stacks in
  the first free slots; returns the leftover that did not fit.
- `remove` drains in slot order, frees emptied slots; returns what left.
- Unknown ids: `add` rejects fully, `remove` yields 0, `find` is null.
- `ItemDef` = 32 B flash each; `ItemStack` = 4 B.

## Footprint & tests

- Firmware `.bin`: **60436 → 60436 (+0 B)**; generated item tables linked
  into host tests only (verified absent from the firmware map).
- Tests: 26 → 27 (`item_test`: generated bank, stacking, slots, weight,
  removal, validators, determinism, selfcheck).

## Next

Player State, then quests (`has_item` conditions, `give_item` effects —
the dialogue `cond`/`effect` bytes finally gain meaning).
