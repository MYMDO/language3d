// Roadmap Phase 11A tests: generated station quest walkthrough, prereqs,
// conditions, claim lifecycle, determinism, validators, selfcheck.
#include "../engine/quest.h"
#include "../engine/item.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>

using namespace l3d;

namespace l3d {
const QuestBank content_quests(); // generated_quests.cpp
}

static const ItemBank ticket_bank() {
    static const char ticket[] = "Station ticket";
    static const ItemDef defs[] = {
        {2, uint8_t(ItemCategory::TICKET), false, 1, 5, ticket, {}, 0},
    };
    static const ItemBank bank{defs, 1};
    return bank;
}

static QuestEvent report(QuestLog<8>& log, const QuestBank& bank,
                         PlayerState<8>& p, uint8_t type, uint16_t tag,
                         uint16_t item, uint16_t count) {
    return quest_report(log, bank, p, 1, type, tag, item, count);
}

int main() {
    const QuestBank bank = content_quests();
    L3D_REQUIRE(bank.count == 1);
    uint16_t bad = 0;
    L3D_REQUIRE(quest_validate_bank(bank, &bad));
    const QuestDef* station = quest_find(bank, 1);
    L3D_REQUIRE(station && station->objective_count == 4);
    L3D_REQUIRE(station->objectives[0].type == uint8_t(ObjectiveType::TALK));
    L3D_REQUIRE(station->objectives[0].tag == 1);
    L3D_REQUIRE(station->objectives[1].type == uint8_t(ObjectiveType::REACH));
    L3D_REQUIRE(station->objectives[2].type == uint8_t(ObjectiveType::COLLECT));
    L3D_REQUIRE(station->objectives[3].type == uint8_t(ObjectiveType::GIVE));
    L3D_REQUIRE(station->objectives[3].npc == 2);
    L3D_REQUIRE(std::strcmp(station->title, "Getting to the Station") == 0);

    QuestLog<8> log{};
    log.init();
    PlayerState<8> p{};
    p.init(0x0100);
    Inventory<8> inv{};
    inv.init();
    p.bind_inventory(&inv);
    const ItemBank items = ticket_bank();

    // --- start rules ---
    L3D_REQUIRE(quest_start(log, bank, 7) == QuestEvent::INVALID);
    L3D_REQUIRE(quest_start(log, bank, 1) == QuestEvent::STARTED);
    L3D_REQUIRE(quest_start(log, bank, 1) == QuestEvent::ALREADY);

    // --- wrong order / wrong target ignored ---
    using OT = ObjectiveType;
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::REACH), 3, 0, 0) == QuestEvent::IGNORED);
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::TALK), 2, 0, 0) == QuestEvent::IGNORED);

    // --- 1. TALK Anna ---
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::TALK), 1, 0, 0) == QuestEvent::OBJECTIVE_DONE);
    // --- 2. REACH station district ---
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::REACH), 3, 0, 0) == QuestEvent::OBJECTIVE_DONE);
    // --- 3. COLLECT ticket ---
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::COLLECT), 0, 2, 1) == QuestEvent::OBJECTIVE_DONE);
    // --- 4. GIVE without ticket: no possession, ignored ---
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::GIVE), 2, 2, 1) == QuestEvent::IGNORED);
    L3D_REQUIRE(inv.add(items, 2, 1) == 0);
    L3D_REQUIRE(report(log, bank, p, uint8_t(OT::GIVE), 2, 2, 1) == QuestEvent::QUEST_COMPLETED);
    // --- claim lifecycle ---
    L3D_REQUIRE(quest_claim(log, 1) == QuestEvent::CLAIMED);
    L3D_REQUIRE(quest_claim(log, 1) == QuestEvent::ALREADY);
    L3D_REQUIRE(quest_claim(log, 9) == QuestEvent::INVALID);

    // --- conditions: flag-gated objective ---
    {
        static const char t[] = "Gate";
        static const char d[] = "Flag gate.";
        QuestCondition cond{};
        cond.type = uint8_t(CondType::FLAG_SET);
        cond.flag = 4;
        static const QuestObjective objs[] = {
            {uint8_t(OT::TALK), 1, 0, 1, 0, cond},
        };
        static const QuestDef gated{2, t, d, 0, {}, 1, {objs[0]}};
        static const QuestBank bank2{&gated, 1};
        L3D_REQUIRE(quest_validate_bank(bank2, nullptr));
        QuestLog<4> log2{};
        log2.init();
        PlayerState<4> p2{};
        p2.init(0x0200);
        L3D_REQUIRE(quest_start(log2, bank2, 2) == QuestEvent::STARTED);
        L3D_REQUIRE(quest_report(log2, bank2, p2, 2, uint8_t(OT::TALK), 1, 0, 0) ==
                    QuestEvent::CONDITION_UNMET);
        L3D_REQUIRE(p2.set_flag(4));
        L3D_REQUIRE(quest_report(log2, bank2, p2, 2, uint8_t(OT::TALK), 1, 0, 0) ==
                    QuestEvent::QUEST_COMPLETED);
    }

    // --- prerequisites: quest 3 needs quest 1 CLAIMED ---
    {
        static const char t[] = "Sequel";
        static const char d[] = "Needs the station quest claimed.";
        static const QuestPrereq pre[] = {{1, uint8_t(QuestState::CLAIMED)}};
        static const QuestObjective objs[] = {
            {uint8_t(OT::INSPECT), 9, 0, 1, 0, {}},
        };
        static const QuestDef seq{3, t, d, 1, {pre[0]}, 1, {objs[0]}};
        static const QuestDef both[] = {*station, seq};
        static const QuestBank bank3{both, 2};
        L3D_REQUIRE(quest_validate_bank(bank3, nullptr));
        QuestLog<4> log3{};
        log3.init();
        PlayerState<4> p3{};
        p3.init(0x0300);
        L3D_REQUIRE(quest_start(log3, bank3, 3) == QuestEvent::PREREQ_UNMET);
        // Play quest 1 to CLAIMED inside this log.
        L3D_REQUIRE(quest_start(log3, bank3, 1) == QuestEvent::STARTED);
        Inventory<4> inv3{};
        inv3.init();
        p3.bind_inventory(&inv3);
        L3D_REQUIRE(inv3.add(items, 2, 1) == 0);
        L3D_REQUIRE(quest_report(log3, bank3, p3, 1, uint8_t(OT::TALK), 1, 0, 0) ==
                    QuestEvent::OBJECTIVE_DONE);
        L3D_REQUIRE(quest_report(log3, bank3, p3, 1, uint8_t(OT::REACH), 3, 0, 0) ==
                    QuestEvent::OBJECTIVE_DONE);
        L3D_REQUIRE(quest_report(log3, bank3, p3, 1, uint8_t(OT::COLLECT), 0, 2, 1) ==
                    QuestEvent::OBJECTIVE_DONE);
        L3D_REQUIRE(quest_report(log3, bank3, p3, 1, uint8_t(OT::GIVE), 2, 2, 1) ==
                    QuestEvent::QUEST_COMPLETED);
        L3D_REQUIRE(quest_claim(log3, 1) == QuestEvent::CLAIMED);
        L3D_REQUIRE(quest_start(log3, bank3, 3) == QuestEvent::STARTED);
        L3D_REQUIRE(quest_report(log3, bank3, p3, 3, uint8_t(OT::INSPECT), 9, 0, 0) ==
                    QuestEvent::QUEST_COMPLETED);
    }

    // --- COLLECT quota accumulates via PROGRESS ---
    {
        static const char t[] = "Gather";
        static const char d[] = "Collect three.";
        static const QuestObjective objs[] = {
            {uint8_t(OT::COLLECT), 0, 2, 3, 0, {}},
        };
        static const QuestDef gather{7, t, d, 0, {}, 1, {objs[0]}};
        static const QuestBank bank4{&gather, 1};
        QuestLog<4> log4{};
        log4.init();
        PlayerState<4> p4{};
        p4.init(0x0400);
        L3D_REQUIRE(quest_start(log4, bank4, 7) == QuestEvent::STARTED);
        L3D_REQUIRE(quest_report(log4, bank4, p4, 7, uint8_t(OT::COLLECT), 0, 2, 1) ==
                    QuestEvent::PROGRESS);
        L3D_REQUIRE(quest_report(log4, bank4, p4, 7, uint8_t(OT::COLLECT), 0, 9, 1) ==
                    QuestEvent::IGNORED); // wrong item
        L3D_REQUIRE(quest_report(log4, bank4, p4, 7, uint8_t(OT::COLLECT), 0, 2, 2) ==
                    QuestEvent::QUEST_COMPLETED);
    }

    // --- determinism: same event stream, same log state ---
    {
        QuestLog<8> a{}, b{};
        a.init();
        b.init();
        PlayerState<8> pa{}, pb{};
        pa.init(0x0100);
        pb.init(0x0100);
        quest_start(a, bank, 1);
        quest_start(b, bank, 1);
        for (int run = 0; run < 2; ++run) {
            QuestLog<8>& l = (run == 0) ? a : b;
            PlayerState<8>& pp = (run == 0) ? pa : pb;
            quest_report(l, bank, pp, 1, uint8_t(OT::TALK), 1, 0, 0);
            quest_report(l, bank, pp, 1, uint8_t(OT::REACH), 3, 0, 0);
        }
        const QuestRuntime* ra = a.find(1);
        const QuestRuntime* rb = b.find(1);
        L3D_REQUIRE(ra && rb && ra->state == rb->state);
        L3D_REQUIRE(ra->objective_idx == rb->objective_idx && ra->progress == rb->progress);
    }

    // --- validator negatives ---
    {
        static const char t[] = "Bad";
        static const char d[] = "Bad quest.";
        static const QuestDef empty{5, t, d, 0, {}, 0, {}};
        L3D_REQUIRE(!quest_validate(empty)); // no objectives
        static const QuestDef bad_type{6, t, d, 0, {}, 1, {{9, 0, 0, 1, 0, {}}}};
        L3D_REQUIRE(!quest_validate(bad_type));
        static const QuestDef dup_bank_defs[] = {*station, *station};
        const QuestBank dup_bank{dup_bank_defs, 2};
        uint16_t bad_id = 0;
        L3D_REQUIRE(!quest_validate_bank(dup_bank, &bad_id) && bad_id == 1);
    }

    L3D_REQUIRE(quest_selfcheck());

    std::printf("quest OK\n");
    return 0;
}
