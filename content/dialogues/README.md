# Dialogue content format (.dlg)

Strict line-based format compiled by `tools/build_dialogue.py` into static
C++ tables. The build FAILS on any validation error (duplicate ids,
dangling choice targets, bad enums, over-long text) — invalid content
breaks CI by construction.

```text
dialogue <u16 id> npc=<u16 npcTag> [variant=<u16 group>]
         [require=<word>:<0..4 mastery>]
node <u16 nodeId> speaker=<npc|player|narrator> lang=<und|en|de|pl|es|fr>
     cefr=<A1|A2|B1|B2|C1|C2>
     [vocab=<u16,… up to 4>] [grammar=<u16,… up to 4>]
     [cond=<NONE|HAS_ITEM:i:c|FLAG_SET:b|COUNTER_GE:i:t|LEVEL_GE:l>]
     [effect=<NONE|GIVE_ITEM:i:c|SET_FLAG:b|ADD_COUNTER:i:n|ADD_XP:n|START_QUEST:q>]
     [expect=<primary[:secondary]>]
text <1..192 chars, single line>
choice "<text>" -> <nodeId|END>     # up to 4 per node
       [intent=<u16, 0 = none>]
       [vocab=<u16,… up to 4>] [grammar=<u16,… up to 4>]
node … [expect=<primary[:secondary]>]  # accepted response intents
```

Adaptive variants: dialogues sharing nonzero `variant` form one
conversation in several difficulty tiers; `require` gates a tier on
word mastery (see `dq_select_variant`). Variant-less dialogues behave
exactly as before.

Rules:

- Node ids unique per dialogue; ≤ 16 nodes, ≤ 4 choices each.
- A node with no `choice` lines is terminal (arrival completes).
- `choice -> END` ends the dialogue immediately when taken.
- `cond` gates node entry (evaluated by the orchestration layer, inert in
  the dialogue engine); `effect` applies on node entry (rewards, flags,
  quest starts — never quest internals, which stay in the quest engine).
- Response evaluation (deterministic, no LLM): a choice's `intent` is
  matched against the entry node's `expect` set — primary → CORRECT,
  secondary → PARTIAL, otherwise INCORRECT. Nodes without `expect` keep
  the legacy USED-only path. Shared intent ids (content namespace):
  1=request-ticket, 2=smalltalk, 3=offtopic, 4=give-directions,
  5=decline, 6=offer-help. Authoring rule: choice 1 should be the
  constructive answer carrying the primary intent.
- `vocab`/`grammar` are opaque content ids (language phase resolves them).
- `#` starts a comment; blank lines ignored.
- Texts may not contain `"` (choices) or newlines; C++-escaped by the tool.
