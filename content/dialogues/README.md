# Dialogue content format (.dlg)

Strict line-based format compiled by `tools/build_dialogue.py` into static
C++ tables. The build FAILS on any validation error (duplicate ids,
dangling choice targets, bad enums, over-long text) — invalid content
breaks CI by construction.

```text
dialogue <u16 id> npc=<u16 npcTag>
node <u16 nodeId> speaker=<npc|player|narrator> lang=<und|en|de|pl|es|fr>
     cefr=<A1|A2|B1|B2|C1|C2>
     [vocab=<u16,… up to 4>] [grammar=<u16,… up to 4>]
     [cond=<NONE|HAS_ITEM:i:c|FLAG_SET:b|COUNTER_GE:i:t|LEVEL_GE:l>]
     [effect=<NONE|GIVE_ITEM:i:c|SET_FLAG:b|ADD_COUNTER:i:n|ADD_XP:n|START_QUEST:q>]
text <1..192 chars, single line>
choice "<text>" -> <nodeId|END>     # up to 4 per node
```

Rules:

- Node ids unique per dialogue; ≤ 16 nodes, ≤ 4 choices each.
- A node with no `choice` lines is terminal (arrival completes).
- `choice -> END` ends the dialogue immediately when taken.
- `cond` gates node entry (evaluated by the orchestration layer, inert in
  the dialogue engine); `effect` applies on node entry (rewards, flags,
  quest starts — never quest internals, which stay in the quest engine).
- `vocab`/`grammar` are opaque content ids (language phase resolves them).
- `#` starts a comment; blank lines ignored.
- Texts may not contain `"` (choices) or newlines; C++-escaped by the tool.
