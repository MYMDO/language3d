# Dialogue foundation (Roadmap Phase 8, v0.42.0)

Status: implemented. Definitions, validation, session state machine and
one real English scenario. No quests, inventory, scoring or adaptive
difficulty — language metadata is carried but inert.

## Data-driven pipeline (no trees in C++)

```text
content/dialogues/*.dlg   human-editable strict text format
        ↓  tools/build_dialogue.py (parse + validate + emit)
build/generated_dialogue.cpp   static tables (flash-resident)
        ↓  zero-copy views (engine/dialogue.h)
DialogueBank → DialogueDef → DialogueNode → Choice + LangMeta
```

Invalid content (duplicate ids, dangling `-> node`, bad enums, over-long
text) fails the generator, which fails the build on Linux/Windows and the
dedicated `validate-content` CI job. A runtime validator
(`dialogue_validate*`, same rules) guards hand-built/programmatic defs.

Format: `content/dialogues/README.md`. Caps: 16 nodes/def, 4 choices/node,
192 text chars, 4 vocab + 4 grammar tags/node. Since v0.50.0, choices also
carry `intent` + vocab/grammar tags and nodes declare `expect` sets; see
response-evaluation.md (purely additive fields — old tables compile
unchanged).

## Session separation (explicit requirement)
```text
InteractSession:  active / ended            (proximity + E edge)
DialogueSession:  active / completed / aborted   (talk itself)
```

Two independent structs, two lifecycles. Walking away aborts dialogue
(`ABORTED`) without destroying the interaction record; an ended
interaction never implicitly kills dialogue state — the owner pairs them.
Terminal nodes (no choices) and `-> END` choices complete the session;
bad indices and non-ACTIVE sessions reject input deterministically.

## Language metadata (inert extension point)

Every node carries `{lang, cefr, vocab[4], grammar[4]}` plus structured
`cond`/`effect` records. Language metadata stays inert (validated, never
scored — the language phase). Since v0.46.0, `cond`/`effect` are evaluated
and applied by the orchestration layer (`engine/dialogue_quest.h`), never
by the dialogue engine itself; see quest-integration.md.
the language phase will score/branch on these fields without reshaping
this subsystem. Language-agnostic by construction (`und/en/de/pl/es/fr`
today; Ukrainian→X by adding rows, never gameplay branches).

## NPC binding

`DialogueDef.npc_tag` lives in the `NPCAgent::nameTag` content namespace;
the session stores the runtime NPC id handoff from `InteractSession.npc`.
Sentinel ids are rejected; NPC liveness stays the owner's discipline
(same rule as Entity/NPC pools).

## Footprint & tests

- Firmware `.bin`: **60436 → 60436 (+0 B)**; generated content tables are
  linked into host tests only (verified absent from the firmware map).
- Tests: 25 → 26 (`dialogue_test`: generated Anna scenario traversal ×2
  paths, abort, bad index, NPC rules, determinism, validator negatives,
  selfcheck).

## Next (Phase 9 — Items / Inventory)

Item/Inventory fixed pools following the Entity-pool pattern; dialogue
`effect` bytes become meaningful when quests arrive.
