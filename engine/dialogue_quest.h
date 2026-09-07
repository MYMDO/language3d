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
#include "language.h" // profile/bank for verdict scoring (one-way)

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

// Deterministic response verdict. Pure function of the entered node's
// expectation set and the choice's declared intent — never of the choice
// position. Nodes without expectations yield NONE (legacy USED-only path).
enum class ResponseVerdict : uint8_t { NONE = 0, CORRECT, PARTIAL, INCORRECT };

inline ResponseVerdict dq_evaluate(const DialogueNode& node,
                                   const DialogueChoice& choice) {
    if (node.expect_primary == 0 && node.expect_secondary == 0)
        return ResponseVerdict::NONE;
    if (choice.intent != 0 && choice.intent == node.expect_primary)
        return ResponseVerdict::CORRECT;
    if (choice.intent != 0 && choice.intent == node.expect_secondary)
        return ResponseVerdict::PARTIAL;
    return ResponseVerdict::INCORRECT;
}

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

// Needs the language profile + bank: verdicts feed VocabularyProgress.
#include "language.h"

struct DialogueStep {
    bool advanced{false};
    uint8_t session_state{uint8_t(DialogueState::EMPTY)};
    EffectResult effect{};
    bool cond_rejected{false}; // target node cond unmet: session unchanged
    uint8_t verdict{uint8_t(ResponseVerdict::NONE)};
};

// Advance by choice through the cond gate, evaluate the response, record
// its vocabulary (USED always; CORRECT/INCORRECT per verdict), then apply
// the entered effect. CORRECT/PARTIAL advance; INCORRECT holds the session
// on the current node (the NPC asks again) — the world consequence of a
// wrong answer. Legacy nodes (no expectations) advance with USED only.
template <size_t QN, size_t INV, size_t LN>
DialogueStep dq_choose(DialogueSession& s, const DialogueBank& dbank,
                       PlayerState<INV>& p, QuestLog<QN>& log,
                       const QuestBank& qbank, const ItemBank& items,
                       LanguageProfile<LN>& lang, const VocabularyBank& vbank,
                       size_t choice) {
    DialogueStep step{};
    step.session_state = s.state;
    if (s.state != uint8_t(DialogueState::ACTIVE)) return step;
    const DialogueDef* d = dialogue_find(dbank, s.dialogue);
    if (!d) return step;
    const DialogueNode* at = dialogue_find_node(*d, s.node);
    if (!at || choice >= at->choice_count) return step;
    const DialogueChoice& ch = at->choices[choice];
    const uint16_t next = ch.next;
    if (next != DIALOGUE_NONE) {
        const DialogueNode* to = dialogue_find_node(*d, next);
        if (!to) return step; // validated content never hits this
        if (!dq_cond_met(to->cond, p)) {
            step.cond_rejected = true;
            return step;
        }
    }
    const ResponseVerdict verdict = dq_evaluate(*at, ch);
    step.verdict = uint8_t(verdict);
    // The player's pick engages the choice vocabulary; the verdict scores it.
    for (size_t i = 0; i < ch.vocab_count; ++i) {
        lang.record(vbank, ch.vocab[i], LanguageEvent::USED);
        if (verdict == ResponseVerdict::CORRECT)
            lang.record(vbank, ch.vocab[i], LanguageEvent::CORRECT);
        else if (verdict == ResponseVerdict::INCORRECT)
            lang.record(vbank, ch.vocab[i], LanguageEvent::INCORRECT);
    }
    if (verdict == ResponseVerdict::INCORRECT) {
        step.session_state = s.state; // stay: NPC asks again
        return step;
    }
    if (!dialogue_choose(s, dbank, choice)) return step;
    step.advanced = true;
    step.session_state = s.state;
    const DialogueNode* now = dialogue_find_node(*d, s.node);
    if (now) step.effect = dq_apply_effect(now->effect, p, log, qbank, items);
    return step;
}

// Deterministic adaptive variant selection (rules-based content selection,
// not semantic/NLP/LLM evaluation). Among the dialogues sharing a variant
// group, picks the eligible candidate with the highest required mastery;
// ties break toward the lowest dialogue id. A requirement-free candidate
// is the guaranteed fallback; with no group members (or none eligible and
// no fallback) returns DIALOGUE_NONE. Legacy group-0 dialogues are never
// selected through this function — open them by id as before.
template <size_t LN>
uint16_t dq_select_variant(const LanguageProfile<LN>& profile,
                           const DialogueBank& bank, uint16_t group) {
    if (group == 0) return DIALOGUE_NONE;
    uint16_t best = DIALOGUE_NONE;
    uint8_t best_level = 0;
    bool have_best = false;
    for (size_t i = 0; i < bank.count; ++i) {
        const DialogueDef& d = bank.defs[i];
        if (d.variant_group != group) continue;
        bool eligible;
        uint8_t level = 0;
        if (d.require_word == 0) {
            eligible = true; // fallback candidate
        } else {
            const VocabularyProgress* s = profile.progress_of(d.require_word);
            level = d.require_mastery;
            eligible = (s && uint8_t(mastery_of(*s)) >= d.require_mastery);
        }
        if (!eligible) continue;
        if (!have_best || level > best_level ||
            (level == best_level && d.id < best)) {
            best = d.id;
            best_level = level;
            have_best = true;
        }
    }
    return best;
}

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool dq_selfcheck();

} // namespace l3d
