#pragma once
// Roadmap Phase 11A: quest foundation (no rewards, no dialogue integration).
//
// The quest engine sees ONLY objective / condition / state. It never
// includes dialogue/npc/schedule/world units: tags are opaque content ids,
// and player facts (items, flags, counters, level) arrive through the
// PlayerState association. Include direction is strictly one-way
// (quest -> player -> item); orchestration of Dialogue x Quest x Inventory
// belongs to a future Game/Simulation layer, never to a QuestManager god
// object. Rewards and dialogue cond/effect wiring arrive in Phase 11B.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "player.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t QUEST_NONE = 0xFFFFu;
constexpr size_t QUEST_MAX_OBJECTIVES = 8;
constexpr size_t QUEST_MAX_PREREQS = 2;
constexpr size_t QUEST_MAX_TITLE = 128;
constexpr size_t QUEST_MAX_DESC = 256;

enum class QuestState : uint8_t { NOT_STARTED = 0, ACTIVE, COMPLETED, CLAIMED };

enum class ObjectiveType : uint8_t {
    UNKNOWN = 0,
    TALK,    // tag = npc_tag
    REACH,   // tag = location_tag
    COLLECT, // item + count
    GIVE,    // item + count + npc = recipient tag (possession checked, not consumed yet)
    USE,     // item (+ tag = optional location context)
    INSPECT, // tag = target tag
    COUNT
};

enum class CondType : uint8_t {
    ALWAYS = 0,
    HAS_ITEM, // item + count
    FLAG_SET, // flag bit
    COUNTER_GE, // counter idx + threshold
    LEVEL_GE,   // level
    COUNT
};

struct QuestCondition {
    uint8_t type{uint8_t(CondType::ALWAYS)};
    uint16_t item{0};
    uint16_t count{0};
    uint8_t flag{0};
    uint8_t counter{0};
    uint16_t threshold{0};
    uint8_t level{0};
};

struct QuestObjective {
    uint8_t type{uint8_t(ObjectiveType::UNKNOWN)};
    uint16_t tag{0};   // TALK/REACH/INSPECT target, USE context
    uint16_t item{0};  // COLLECT/GIVE/USE item
    uint16_t count{1}; // COLLECT/GIVE quota
    uint16_t npc{0};   // GIVE recipient tag
    QuestCondition cond{}; // checked when the objective would complete
};

struct QuestPrereq {
    uint16_t quest{QUEST_NONE};
    uint8_t state{uint8_t(QuestState::COMPLETED)}; // minimum required state
};

struct QuestDef {
    uint16_t id{0};
    const char* title{nullptr};
    const char* description{nullptr};
    uint8_t prereq_count{0};
    QuestPrereq prereqs[QUEST_MAX_PREREQS]{};
    uint8_t objective_count{0};
    QuestObjective objectives[QUEST_MAX_OBJECTIVES]{};
};

struct QuestBank {
    const QuestDef* defs{nullptr};
    size_t count{0};
};

// Runtime per-quest progress (caller-owned log, fixed slots).
struct QuestRuntime {
    uint16_t def{QUEST_NONE}; // quest def id; NONE = free slot
    uint8_t state{uint8_t(QuestState::NOT_STARTED)};
    uint8_t objective_idx{0};
    uint16_t progress{0}; // accumulated count toward current objective
};

template <size_t N>
struct QuestLog {
    static_assert(N >= 1 && N <= 256, "quest log capacity out of range");
    QuestRuntime slots[N]{};
    size_t tracked{0};

    void init() {
        for (size_t i = 0; i < N; ++i) slots[i] = QuestRuntime{};
        tracked = 0;
    }
    QuestRuntime* find(uint16_t def) {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].def == def) return &slots[i];
        }
        return nullptr;
    }
    const QuestRuntime* find(uint16_t def) const {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].def == def) return &slots[i];
        }
        return nullptr;
    }
    QuestRuntime* track(uint16_t def) {
        if (QuestRuntime* r = find(def)) return r;
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].def == QUEST_NONE) {
                slots[i] = QuestRuntime{};
                slots[i].def = def;
                ++tracked;
                return &slots[i];
            }
        }
        return nullptr; // log full
    }
};

enum class QuestEvent : uint8_t {
    IGNORED = 0, // no active matching objective (wrong order/target/quest)
    STARTED,     // quest moved NOT_STARTED -> ACTIVE
    PROGRESS,    // COLLECT quota advanced, objective still open
    OBJECTIVE_DONE,
    QUEST_COMPLETED,
    CLAIMED,         // COMPLETED -> CLAIMED acknowledged (no payload yet)
    CONDITION_UNMET, // match found but its condition failed
    PREREQ_UNMET,
    ALREADY, // start an ACTIVE quest / claim a CLAIMED one
    INVALID  // unknown quest, log full, bad state transition
};

const QuestDef* quest_find(const QuestBank& bank, uint16_t id);
bool quest_validate(const QuestDef& def);
bool quest_validate_bank(const QuestBank& bank, uint16_t* bad_id);

// Condition truth against player facts (inventory via association).
template <size_t INV>
bool quest_condition_met(const QuestCondition& c, const PlayerState<INV>& p) {
    switch (c.type) {
        case uint8_t(CondType::ALWAYS):
            return true;
        case uint8_t(CondType::HAS_ITEM):
            return p.inventory && p.inventory->has(c.item, c.count);
        case uint8_t(CondType::FLAG_SET):
            return p.has_flag(c.flag);
        case uint8_t(CondType::COUNTER_GE):
            return p.counter(c.counter) >= c.threshold;
        case uint8_t(CondType::LEVEL_GE):
            return p.level >= c.level;
        default:
            return false;
    }
}

template <size_t N>
QuestEvent quest_start(QuestLog<N>& log, const QuestBank& bank,
                       uint16_t def) {
    const QuestDef* d = quest_find(bank, def);
    if (!d) return QuestEvent::INVALID;
    QuestRuntime* r = log.find(def);
    if (r && r->state != uint8_t(QuestState::NOT_STARTED))
        return QuestEvent::ALREADY;
    // Prerequisites: referenced quests must reach at least the minimum.
    for (size_t i = 0; i < d->prereq_count; ++i) {
        const QuestRuntime* pre = log.find(d->prereqs[i].quest);
        const uint8_t have =
            pre ? pre->state : uint8_t(QuestState::NOT_STARTED);
        if (have < d->prereqs[i].state) return QuestEvent::PREREQ_UNMET;
    }
    r = log.track(def);
    if (!r) return QuestEvent::INVALID; // log full
    r->state = uint8_t(QuestState::ACTIVE);
    r->objective_idx = 0;
    r->progress = 0;
    return QuestEvent::STARTED;
}

// Report a world event; advances the matching ACTIVE objective, if any.
// For COLLECT, count accumulates toward the quota (PROGRESS until done).
template <size_t N, size_t INV>
QuestEvent quest_report(QuestLog<N>& log, const QuestBank& bank,
                        const PlayerState<INV>& p, uint16_t def, uint8_t type,
                        uint16_t tag, uint16_t item, uint16_t count) {
    const QuestDef* d = quest_find(bank, def);
    QuestRuntime* r = d ? log.find(def) : nullptr;
    if (!d || !r || r->state != uint8_t(QuestState::ACTIVE))
        return QuestEvent::IGNORED;
    if (r->objective_idx >= d->objective_count) return QuestEvent::IGNORED;
    const QuestObjective& o = d->objectives[r->objective_idx];
    if (o.type != type) return QuestEvent::IGNORED;
    // Target matching per objective kind.
    bool match = false;
    switch (o.type) {
        case uint8_t(ObjectiveType::TALK):
        case uint8_t(ObjectiveType::REACH):
        case uint8_t(ObjectiveType::INSPECT):
            match = (o.tag == tag);
            break;
        case uint8_t(ObjectiveType::COLLECT):
            match = (o.item == item && count > 0);
            break;
        case uint8_t(ObjectiveType::GIVE):
            match = (o.item == item && o.npc == tag &&
                     p.inventory && p.inventory->has(item, o.count));
            break;
        case uint8_t(ObjectiveType::USE):
            match = (o.item == item && (o.tag == 0 || o.tag == tag));
            break;
        default:
            return QuestEvent::IGNORED;
    }
    if (!match) return QuestEvent::IGNORED;
    if (!quest_condition_met(o.cond, p)) return QuestEvent::CONDITION_UNMET;
    if (o.type == uint8_t(ObjectiveType::COLLECT)) {
        uint32_t total = uint32_t(r->progress) + uint32_t(count);
        if (total > 0xFFFFu) total = 0xFFFFu;
        r->progress = uint16_t(total);
        if (r->progress < o.count) return QuestEvent::PROGRESS;
    }
    // Objective complete: advance or finish the quest.
    r->progress = 0;
    ++r->objective_idx;
    if (r->objective_idx >= d->objective_count) {
        r->state = uint8_t(QuestState::COMPLETED);
        return QuestEvent::QUEST_COMPLETED;
    }
    return QuestEvent::OBJECTIVE_DONE;
}

// COMPLETED -> CLAIMED (rewards arrive in Phase 11B; the transition is real).
template <size_t N>
QuestEvent quest_claim(QuestLog<N>& log, uint16_t def) {
    QuestRuntime* r = log.find(def);
    if (!r) return QuestEvent::INVALID;
    if (r->state == uint8_t(QuestState::CLAIMED)) return QuestEvent::ALREADY;
    if (r->state != uint8_t(QuestState::COMPLETED)) return QuestEvent::INVALID;
    r->state = uint8_t(QuestState::CLAIMED);
    return QuestEvent::CLAIMED;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool quest_selfcheck();

} // namespace l3d
