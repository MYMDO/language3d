// Roadmap Phase 11A translation unit: lookup, validators, self-check.
// Unreferenced, quest_selfcheck() is dropped by --gc-sections.
#include "quest.h"

namespace l3d {

const QuestDef* quest_find(const QuestBank& bank, uint16_t id) {
    if (!bank.defs || id == QUEST_NONE) return nullptr;
    for (size_t i = 0; i < bank.count; ++i) {
        if (bank.defs[i].id == id) return &bank.defs[i];
    }
    return nullptr;
}

static bool cond_valid(const QuestCondition& c) {
    if (c.type >= uint8_t(CondType::COUNT)) return false;
    if (c.type == uint8_t(CondType::FLAG_SET) && c.flag >= 32) return false;
    if (c.type == uint8_t(CondType::COUNTER_GE) &&
        c.counter >= PLAYER_COUNTERS)
        return false;
    return true;
}

bool quest_validate(const QuestDef& def) {
    if (def.id == 0 || def.id == QUEST_NONE) return false;
    if (!def.title || !def.title[0] || !def.description || !def.description[0])
        return false;
    size_t len = 0;
    while (def.title[len] && len <= QUEST_MAX_TITLE) ++len;
    if (len == 0 || len > QUEST_MAX_TITLE) return false;
    len = 0;
    while (def.description[len] && len <= QUEST_MAX_DESC) ++len;
    if (len == 0 || len > QUEST_MAX_DESC) return false;
    if (def.prereq_count > QUEST_MAX_PREREQS) return false;
    for (size_t i = 0; i < def.prereq_count; ++i) {
        if (def.prereqs[i].quest == QUEST_NONE) return false;
        if (def.prereqs[i].state != uint8_t(QuestState::ACTIVE) &&
            def.prereqs[i].state != uint8_t(QuestState::COMPLETED) &&
            def.prereqs[i].state != uint8_t(QuestState::CLAIMED))
            return false;
    }
    if (def.objective_count == 0 || def.objective_count > QUEST_MAX_OBJECTIVES)
        return false;
    for (size_t i = 0; i < def.objective_count; ++i) {
        const QuestObjective& o = def.objectives[i];
        if (o.type == uint8_t(ObjectiveType::UNKNOWN) ||
            o.type >= uint8_t(ObjectiveType::COUNT))
            return false;
        if (!cond_valid(o.cond)) return false;
        if ((o.type == uint8_t(ObjectiveType::COLLECT) ||
             o.type == uint8_t(ObjectiveType::GIVE)) &&
            (o.item == 0 || o.item == ITEM_NONE || o.count == 0))
            return false;
    }
    return true;
}

bool quest_validate_bank(const QuestBank& bank, uint16_t* bad_id) {
    auto fail = [&](uint16_t id) {
        if (bad_id) *bad_id = id;
        return false;
    };
    if (!bank.defs && bank.count != 0) return fail(QUEST_NONE);
    for (size_t i = 0; i < bank.count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (bank.defs[j].id == bank.defs[i].id)
                return fail(bank.defs[i].id);
        }
        if (!quest_validate(bank.defs[i])) return fail(bank.defs[i].id);
    }
    return true;
}

bool quest_selfcheck() {
    static const char title[] = "Errand";
    static const char desc[] = "Talk and fetch.";
    static const QuestObjective objs[] = {
        {uint8_t(ObjectiveType::TALK), 1, 0, 1, 0, {}},
        {uint8_t(ObjectiveType::COLLECT), 0, 7, 2, 0, {}},
    };
    static const QuestDef full{3, title, desc, 0, {}, 2, {objs[0], objs[1]}};
    static const QuestBank bank{&full, 1};
    if (!quest_validate_bank(bank, nullptr)) return false;
    if (quest_find(bank, 4) != nullptr) return false;

    QuestLog<4> log{};
    log.init();
    if (quest_start(log, bank, 3) != QuestEvent::STARTED) return false;
    if (quest_start(log, bank, 3) != QuestEvent::ALREADY) return false;

    PlayerState<4> p{};
    p.init(0x0100);
    // Wrong order: COLLECT reported while TALK is current -> IGNORED.
    if (quest_report(log, bank, p, 3, uint8_t(ObjectiveType::COLLECT), 0, 7, 2) !=
        QuestEvent::IGNORED)
        return false;
    if (quest_report(log, bank, p, 3, uint8_t(ObjectiveType::TALK), 2, 0, 0) !=
        QuestEvent::IGNORED)
        return false; // wrong npc tag
    if (quest_report(log, bank, p, 3, uint8_t(ObjectiveType::TALK), 1, 0, 0) !=
        QuestEvent::OBJECTIVE_DONE)
        return false;
    if (quest_report(log, bank, p, 3, uint8_t(ObjectiveType::COLLECT), 0, 7, 1) !=
        QuestEvent::PROGRESS)
        return false;
    if (quest_report(log, bank, p, 3, uint8_t(ObjectiveType::COLLECT), 0, 7, 1) !=
        QuestEvent::QUEST_COMPLETED)
        return false;
    if (quest_claim(log, 3) != QuestEvent::CLAIMED) return false;
    if (quest_claim(log, 3) != QuestEvent::ALREADY) return false;
    if (quest_claim(log, 4) != QuestEvent::INVALID) return false;
    return true;
}

} // namespace l3d
