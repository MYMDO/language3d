// Roadmap Phase 11B translation unit: deterministic self-check over the
// orchestration glue. Unreferenced, dq_selfcheck() is dropped by
// --gc-sections (firmware) or never pulled from the static lib.
#include "dialogue_quest.h"

namespace l3d {

bool dq_selfcheck() {
    // Dialogue: 1 "Key?" -> 2 (cond HAS_ITEM ticket) -> 3 (effect GIVE key).
    static const char t1[] = "Need a key?";
    static const char c1[] = "Yes.";
    static const char t2[] = "Show ticket first.";
    static const char t3[] = "Here is the key.";
    static const DialogueNode nodes[] = {
        {1, 0, 1, {{c1, 2}}, t1, DialogueLangMeta{}, {}, {}},
        {2, 0, 1, {{c1, 3}}, t2, DialogueLangMeta{},
         {uint8_t(DialogueCondKind::HAS_ITEM), 2, 1}, {}},
        {3, 0, 0, {}, t3, DialogueLangMeta{}, {},
         {uint8_t(DialogueEffectKind::GIVE_ITEM), 3, 1}},
    };
    static const DialogueDef def{1, 1, 3, nodes};
    static const DialogueBank dbank{&def, 1};

    static const char ticket[] = "Ticket";
    static const char key[] = "Key";
    static const ItemDef idefs[] = {
        {2, 0, false, 1, 1, ticket, {}, 0}, // category checked loosely here
        {3, 0, false, 1, 1, key, {}, 0},
    };
    static const ItemBank ibank{idefs, 2};

    static const QuestBank qbank{nullptr, 0};
    QuestLog<4> log{};
    log.init();
    PlayerState<4> p{};
    p.init(0x0100);
    Inventory<4> inv{};
    inv.init();
    p.bind_inventory(&inv);

    DialogueSession s{};
    if (!dq_begin(s, dbank, log, qbank, ibank, p, 1, 0x0100, nullptr))
        return false;
    if (s.node != 1) return false;
    // Node 2 is ticket-gated: advance rejected while ticketless.
    DialogueStep st = dq_choose(s, dbank, p, log, qbank, ibank, 0);
    if (st.advanced || !st.cond_rejected) return false;
    if (s.node != 1) return false;
    // Hand over the ticket, then pass the gate; node 3 gives the key.
    if (inv.add(ibank, 2, 1) != 0) return false;
    st = dq_choose(s, dbank, p, log, qbank, ibank, 0);
    if (!st.advanced || s.node != 2) return false;
    st = dq_choose(s, dbank, p, log, qbank, ibank, 0);
    if (!st.advanced || s.state != uint8_t(DialogueState::COMPLETED))
        return false;
    if (st.effect.kind != uint8_t(EffectApply::APPLIED)) return false;
    if (!inv.has(3, 1)) return false;
    return true;
}

} // namespace l3d
