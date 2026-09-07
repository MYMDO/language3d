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
    LanguageProfile<8> lang{};
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    lang.init(pair);
    static const char w_yes[] = "yes";
    static const char w_no[] = "no";
    static const VocabularyEntry wentries[] = {
        {301, w_yes, uint8_t(DialogueLang::EN), uint8_t(WordPOS::PHRASE),
         uint8_t(DialogueCEFR::A1), "agree"},
        {302, w_no, uint8_t(DialogueLang::EN), uint8_t(WordPOS::PHRASE),
         uint8_t(DialogueCEFR::A1), "disagree"},
    };
    static const VocabularyBank vbank{wentries, 2};

    // --- dq_begin opens + applies entry effect (none here) ---
    DialogueSession s{};
    EffectResult applied{};
    L3D_REQUIRE(dq_begin(s, dbank, log, qbank, items, p, 9, 0x0100, &applied));
    L3D_REQUIRE(s.node == 1 && applied.kind == uint8_t(EffectApply::NONE));

    // --- cond gate: node 2 needs the ticket ---
    DialogueStep st = dq_choose(s, dbank, p, log, qbank, items, lang, vbank, 0);
    L3D_REQUIRE(!st.advanced && st.cond_rejected && s.node == 1);
    // The other branch (node 3) has no cond: passes, applies START_QUEST.
    st = dq_choose(s, dbank, p, log, qbank, items, lang, vbank, 1);
    L3D_REQUIRE(st.advanced && s.state == uint8_t(DialogueState::COMPLETED));
    L3D_REQUIRE(st.effect.kind == uint8_t(EffectApply::QUEST_EVENT));
    // Quest 4 unknown to the empty bank: start rejected, reported through.
    L3D_REQUIRE(st.effect.quest_event == uint8_t(QuestEvent::INVALID));

    // --- effect with the ticket held: key handed over ---
    L3D_REQUIRE(inv.add(items, 2, 1) == 0);
    L3D_REQUIRE(dq_begin(s, dbank, log, qbank, items, p, 9, 0x0100, nullptr));
    st = dq_choose(s, dbank, p, log, qbank, items, lang, vbank, 0);
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

    // --- response evaluation: intent match, not choice position ---
    {
        // Node expects 10 (primary) / 20 (secondary). Choice order is
        // deliberately shuffled: CORRECT sits at index 1.
        static const char q[] = "Right?";
        static const char a0[] = "Maybe.";
        static const char a1[] = "Exactly.";
        static const char a2[] = "Nope.";
        static const DialogueNode enodes[] = {
            {1, 0, 3, {{a0, 2, 20, {302}, 1, {}, 0}, {a1, 2, 10, {301}, 1, {}, 0},
                       {a2, 2, 30, {302}, 1, {}, 0}},
             q, DialogueLangMeta{}, {}, {}, 10, 20},
            {2, 0, 0, {}, q, DialogueLangMeta{}, {}, {}},
        };
        static const DialogueDef edef{11, 7, 2, enodes};
        static const DialogueBank ebank{&edef, 1};
        L3D_REQUIRE(dialogue_validate_bank(ebank, nullptr));
        LanguageProfile<8> lp{};
        lp.init(pair);
        DialogueSession se{};
        L3D_REQUIRE(dq_begin(se, ebank, log, qbank, items, p, 11, 0x0100, nullptr));
        // Index 1 carries the primary intent -> CORRECT + advance.
        DialogueStep e1 = dq_choose(se, ebank, p, log, qbank, items, lp, vbank, 1);
        L3D_REQUIRE(e1.advanced && e1.verdict == uint8_t(ResponseVerdict::CORRECT));
        L3D_REQUIRE(se.node == 2);
        const VocabularyProgress* pr = lp.progress_of(301);
        L3D_REQUIRE(pr && pr->uses == 1 && pr->correct == 1);
        // Reopen: secondary intent at index 0 -> PARTIAL + advance.
        L3D_REQUIRE(dq_begin(se, ebank, log, qbank, items, p, 11, 0x0100, nullptr));
        DialogueStep e0 = dq_choose(se, ebank, p, log, qbank, items, lp, vbank, 0);
        L3D_REQUIRE(e0.advanced && e0.verdict == uint8_t(ResponseVerdict::PARTIAL));
        // Reopen: unrelated intent at index 2 -> INCORRECT, session holds.
        L3D_REQUIRE(dq_begin(se, ebank, log, qbank, items, p, 11, 0x0100, nullptr));
        DialogueStep e2 = dq_choose(se, ebank, p, log, qbank, items, lp, vbank, 2);
        L3D_REQUIRE(!e2.advanced && e2.verdict == uint8_t(ResponseVerdict::INCORRECT));
        L3D_REQUIRE(se.node == 1 && se.state == uint8_t(DialogueState::ACTIVE));
        const VocabularyProgress* pr2 = lp.progress_of(302);
        L3D_REQUIRE(pr2 && pr2->incorrect == 1);
        // Legacy node without expectations: verdict NONE, plain advance.
        L3D_REQUIRE(dq_begin(se, dbank, log, qbank, items, p, 9, 0x0100, nullptr));
        DialogueStep el = dq_choose(se, dbank, p, log, qbank, items, lp, vbank, 1);
        L3D_REQUIRE(el.advanced && el.verdict == uint8_t(ResponseVerdict::NONE));
    }

    L3D_REQUIRE(dq_selfcheck());

    std::printf("dialogue-quest OK\n");
    return 0;
}
