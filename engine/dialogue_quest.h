#pragma once
// Roadmap Phase 11B: dialogue x quest x inventory orchestration glue.
//
// This is the FIRST cross-subsystem unit, and it exists precisely so that
// none of the engines has to know about the others: dialogue.h never
// includes quest.h (or vice versa); both stay unaware of the composition.
// All meetings happen here, as explicit free functions over shared ids:
//
//   dq_begin   dialogue open + TALK report to every ACTIVE quest
//   dq_choose  cond-gated advance + entered-node effect application
//   dq_cond_met / dq_apply_effect  the two primitives behind them
//
// Effect semantics: GIVE_ITEM returns the leftover (caller decides what a
// partial handoff means); SET_FLAG/ADD_COUNTER/ADD_XP apply directly;
// START_QUEST forwards to quest_start and reports its event. Dialogue
// choice navigation itself is untouched (dialogue_choose).
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "dialogue.h"
#include "quest.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

enum class EffectApply : uint8_t {
    NONE = 0,    // no effect on the node
    APPLIED,     // fully applied
    PARTIAL,     // GIVE_ITEM with leftover (effect_left > 0)
    QUEST_EVENT  // START_QUEST: see quest_event
};

struct EffectResult {
    uint8_t kind{uint8_t(EffectApply::NONE)};
    uint16_t left{0};        // GIVE_ITEM leftover
    uint8_t quest_event{uint8_t(QuestEvent::IGNORED)}; // START_QUEST outcome
};

// Node-entry condition truth against player facts.
template <size_t INV>
bool dq_cond_met(const DialogueCond& c, const PlayerState<INV>& p) {
    switch (c.kind) {
        case uint8_t(DialogueCondKind::NONE):
            return true;
        case uint8_t(DialogueCondKind::HAS_ITEM):
            return p.inventory && p.inventory->has(c.p1, c.p2);
        case uint8_t(DialogueCondKind::FLAG_SET):
            return p.has_flag(size_t(c.p1));
        case uint8_t(DialogueCondKind::COUNTER_GE):
            return p.counter(size_t(c.p1)) >= c.p2;
        case uint8_t(DialogueCondKind::LEVEL_GE):
            return p.level >= c.p1;
        default:
            return false;
    }
}

// Apply one entered-node effect. START_QUEST needs the quest log + bank.
template <size_t QN, size_t INV>
EffectResult dq_apply_effect(const DialogueEffect& e, PlayerState<INV>& p,
                             QuestLog<QN>& log, const QuestBank& bank,
                             const ItemBank& items) {
    EffectResult r{};
    switch (e.kind) {
        case uint8_t(DialogueEffectKind::NONE):
            break;
        case uint8_t(DialogueEffectKind::GIVE_ITEM): {
            if (!p.inventory) {
                r.kind = uint8_t(EffectApply::PARTIAL);
                r.left = e.p2;
                break;
            }
            r.left = p.inventory->add(items, e.p1, e.p2);
            r.kind = (r.left == 0) ? uint8_t(EffectApply::APPLIED)
                                   : uint8_t(EffectApply::PARTIAL);
            break;
        }
        case uint8_t(DialogueEffectKind::SET_FLAG):
            p.set_flag(size_t(e.p1));
            r.kind = uint8_t(EffectApply::APPLIED);
            break;
        case uint8_t(DialogueEffectKind::ADD_COUNTER):
            p.add_counter(size_t(e.p1), e.p2);
            r.kind = uint8_t(EffectApply::APPLIED);
            break;
        case uint8_t(DialogueEffectKind::ADD_XP):
            p.add_xp(e.p1);
            r.kind = uint8_t(EffectApply::APPLIED);
            break;
        case uint8_t(DialogueEffectKind::START_QUEST):
            r.kind = uint8_t(EffectApply::QUEST_EVENT);
            r.quest_event = uint8_t(quest_start(log, bank, e.p1));
            break;
        default:
            break;
    }
    return r;
}

// Open dialogue + report TALK(npc_tag) to every ACTIVE quest + apply the
// entry node's effect. Returns false when the dialogue itself won't open.
template <size_t QN, size_t INV>
bool dq_begin(DialogueSession& s, const DialogueBank& dbank,
              QuestLog<QN>& log, const QuestBank& qbank,
              const ItemBank& items, PlayerState<INV>& p, uint16_t dialogue,
              uint16_t npc, EffectResult* applied = nullptr) {
    if (!dialogue_begin(s, dbank, dialogue, npc)) return false;
    const DialogueDef* d = dialogue_find(dbank, dialogue);
    if (!d) return true; // unreachable for validated banks
    for (size_t i = 0; i < QN; ++i) {
        QuestRuntime& r = log.slots[i];
        if (r.def != QUEST_NONE && r.state == uint8_t(QuestState::ACTIVE)) {
            quest_report(log, qbank, p, r.def, uint8_t(ObjectiveType::TALK),
                         d->npc_tag, 0, 0);
        }
    }
    const DialogueNode* at = dialogue_find_node(*d, s.node);
    if (at && applied)
        *applied = dq_apply_effect(at->effect, p, log, qbank, items);
    return true;
}

struct DialogueStep {
    bool advanced{false};
    uint8_t session_state{uint8_t(DialogueState::EMPTY)};
    EffectResult effect{};
    bool cond_rejected{false}; // target node cond unmet: session unchanged
};

// Advance by choice through the cond gate, then apply the entered effect.
template <size_t QN, size_t INV>
DialogueStep dq_choose(DialogueSession& s, const DialogueBank& dbank,
                       PlayerState<INV>& p, QuestLog<QN>& log,
                       const QuestBank& qbank, const ItemBank& items,
                       size_t choice) {
    DialogueStep step{};
    step.session_state = s.state;
    if (s.state != uint8_t(DialogueState::ACTIVE)) return step;
    const DialogueDef* d = dialogue_find(dbank, s.dialogue);
    if (!d) return step;
    const DialogueNode* at = dialogue_find_node(*d, s.node);
    if (!at || choice >= at->choice_count) return step;
    const uint16_t next = at->choices[choice].next;
    if (next != DIALOGUE_NONE) {
        const DialogueNode* to = dialogue_find_node(*d, next);
        if (!to) return step; // validated content never hits this
        if (!dq_cond_met(to->cond, p)) {
            step.cond_rejected = true;
            return step;
        }
    }
    if (!dialogue_choose(s, dbank, choice)) return step;
    step.advanced = true;
    step.session_state = s.state;
    const DialogueNode* now = dialogue_find_node(*d, s.node);
    if (now) step.effect = dq_apply_effect(now->effect, p, log, qbank, items);
    return step;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool dq_selfcheck();

} // namespace l3d
