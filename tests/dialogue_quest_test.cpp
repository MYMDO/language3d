// Roadmap Phase 11B tests: dialogue x quest x inventory orchestration
// primitives (cond gate, effect application, TALK fan-out on open).
#include "../engine/dialogue_quest.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static const ItemBank ticket_bank() {
    static const char ticket[] = "Station ticket";
    static const char key[] = "Rusty key";
    static const ItemDef defs[] = {
        {2, uint8_t(ItemCategory::TICKET), false, 1, 5, ticket, {}, 0},
        {3, uint8_t(ItemCategory::KEY), false, 1, 5, key, {}, 0},
    };
    static const ItemBank bank{defs, 2};
    return bank;
}

int main() {
    const ItemBank items = ticket_bank();
    // Dialogue 9 (npc_tag 7): node 1 -> node 2 [cond HAS_ITEM ticket].
    // Node 2 effect: GIVE_ITEM key. Node 3 terminal: effect START_QUEST 4.
    static const char t1[] = "Ticket?";
    static const char c1[] = "Here.";
    static const char c2[] = "Later.";
    static const char t2[] = "Thanks.";
    static const char t3[] = "Bye.";
    static const DialogueNode nodes[] = {
        {1, 0, 2, {{c1, 2}, {c2, 3}}, t1, DialogueLangMeta{}, {}, {}},
        {2, 0, 0, {}, t2, DialogueLangMeta{},
         {uint8_t(DialogueCondKind::HAS_ITEM), 2, 1},
         {uint8_t(DialogueEffectKind::GIVE_ITEM), 3, 1}},
        {3, 0, 0, {}, t3, DialogueLangMeta{}, {},
         {uint8_t(DialogueEffectKind::START_QUEST), 4, 0}},
    };
    static const DialogueDef def{9, 7, 3, nodes};
    static const DialogueBank dbank{&def, 1};
    static const QuestBank qbank{nullptr, 0};

    QuestLog<4> log{};
    log.init();
    PlayerState<4> p{};
    p.init(0x0100);
    Inventory<4> inv{};
    inv.init();
    p.bind_inventory(&inv);

    // --- dq_begin opens + applies entry effect (none here) ---
    DialogueSession s{};
    EffectResult applied{};
    L3D_REQUIRE(dq_begin(s, dbank, log, qbank, items, p, 9, 0x0100, &applied));
    L3D_REQUIRE(s.node == 1 && applied.kind == uint8_t(EffectApply::NONE));

    // --- cond gate: node 2 needs the ticket ---
    DialogueStep st = dq_choose(s, dbank, p, log, qbank, items, 0);
    L3D_REQUIRE(!st.advanced && st.cond_rejected && s.node == 1);
    // The other branch (node 3) has no cond: passes, applies START_QUEST.
    st = dq_choose(s, dbank, p, log, qbank, items, 1);
    L3D_REQUIRE(st.advanced && s.state == uint8_t(DialogueState::COMPLETED));
    L3D_REQUIRE(st.effect.kind == uint8_t(EffectApply::QUEST_EVENT));
    // Quest 4 unknown to the empty bank: start rejected, reported through.
    L3D_REQUIRE(st.effect.quest_event == uint8_t(QuestEvent::INVALID));

    // --- effect with the ticket held: key handed over ---
    L3D_REQUIRE(inv.add(items, 2, 1) == 0);
    L3D_REQUIRE(dq_begin(s, dbank, log, qbank, items, p, 9, 0x0100, nullptr));
    st = dq_choose(s, dbank, p, log, qbank, items, 0);
    L3D_REQUIRE(st.advanced && s.node == 2);
    L3D_REQUIRE(st.effect.kind == uint8_t(EffectApply::APPLIED));
    L3D_REQUIRE(inv.has(3, 1)); // key received
    L3D_REQUIRE(s.state == uint8_t(DialogueState::COMPLETED)); // node 2 terminal

    // --- TALK fan-out: dq_begin reports to ACTIVE quests ---
    {
        static const char t[] = "Q";
        static const char d[] = "Talk quest.";
        static const QuestObjective objs[] = {
            {uint8_t(ObjectiveType::TALK), 7, 0, 1, 0, {}},
        };
        static const QuestDef qd{4, t, d, 0, {}, 1, {objs[0]}};
        static const QuestBank qb{&qd, 1};
        QuestLog<4> log2{};
        log2.init();
        L3D_REQUIRE(quest_start(log2, qb, 4) == QuestEvent::STARTED);
        DialogueSession s2{};
        // Opening dialogue 9 (npc_tag 7) completes the TALK objective.
        L3D_REQUIRE(dq_begin(s2, dbank, log2, qb, items, p, 9, 0x0100, nullptr));
        const QuestRuntime* r = log2.find(4);
        L3D_REQUIRE(r && r->state == uint8_t(QuestState::COMPLETED));
    }

    // --- dq_cond_met unit coverage ---
    {
        DialogueCond c{};
        L3D_REQUIRE(dq_cond_met(c, p));
        c.kind = uint8_t(DialogueCondKind::LEVEL_GE);
        c.p1 = 90;
        L3D_REQUIRE(!dq_cond_met(c, p));
        c.kind = 99;
        L3D_REQUIRE(!dq_cond_met(c, p));
    }

    L3D_REQUIRE(dq_selfcheck());

    std::printf("dialogue-quest OK\n");
    return 0;
}
