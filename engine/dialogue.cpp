// Roadmap Phase 8 translation unit: validator + session state machine +
// deterministic self-check. Content tables are generated (never handwritten
// here). Unreferenced, dialogue_selfcheck() is dropped by --gc-sections.
#include "dialogue.h"
#include "player.h" // PLAYER_COUNTERS for validator range checks only

namespace l3d {

bool dialogue_validate(const DialogueDef& def, uint16_t* bad_node) {
    auto fail = [&](uint16_t n) {
        if (bad_node) *bad_node = n;
        return false;
    };
    if (def.node_count == 0 || def.node_count > DIALOGUE_MAX_NODES) return fail(DIALOGUE_NONE);
    if (!def.nodes) return fail(DIALOGUE_NONE);
    for (size_t i = 0; i < def.node_count; ++i) {
        const DialogueNode& n = def.nodes[i];
        for (size_t j = 0; j < i; ++j) {
            if (def.nodes[j].id == n.id) return fail(n.id); // duplicate
        }
        if (!n.text || !n.text[0]) return fail(n.id); // empty text
        size_t len = 0;
        while (n.text[len] && len <= DIALOGUE_MAX_TEXT) ++len;
        if (len == 0 || len > DIALOGUE_MAX_TEXT) return fail(n.id);
        if (n.speaker >= uint8_t(DialogueSpeaker::COUNT)) return fail(n.id);
        if (n.choice_count > DIALOGUE_MAX_CHOICES) return fail(n.id);
        if (n.lang.lang >= uint8_t(DialogueLang::COUNT)) return fail(n.id);
        if (n.lang.cefr > uint8_t(DialogueCEFR::C2)) return fail(n.id);
        if (n.lang.vocab_count > DIALOGUE_MAX_TAGS) return fail(n.id);
        if (n.lang.grammar_count > DIALOGUE_MAX_TAGS) return fail(n.id);
        if (n.cond.kind >= uint8_t(DialogueCondKind::COUNT)) return fail(n.id);
        if (n.effect.kind >= uint8_t(DialogueEffectKind::COUNT)) return fail(n.id);
        if (n.cond.kind == uint8_t(DialogueCondKind::HAS_ITEM) &&
            (n.cond.p1 == 0 || n.cond.p2 == 0))
            return fail(n.id);
        if (n.cond.kind == uint8_t(DialogueCondKind::FLAG_SET) && n.cond.p1 >= 32)
            return fail(n.id);
        if (n.cond.kind == uint8_t(DialogueCondKind::COUNTER_GE) &&
            n.cond.p1 >= PLAYER_COUNTERS)
            return fail(n.id);
        if (n.effect.kind == uint8_t(DialogueEffectKind::GIVE_ITEM) &&
            (n.effect.p1 == 0 || n.effect.p2 == 0))
            return fail(n.id);
        if (n.effect.kind == uint8_t(DialogueEffectKind::SET_FLAG) && n.effect.p1 >= 32)
            return fail(n.id);
        if (n.effect.kind == uint8_t(DialogueEffectKind::ADD_COUNTER) &&
            n.effect.p1 >= PLAYER_COUNTERS)
            return fail(n.id);
        if (n.effect.kind == uint8_t(DialogueEffectKind::START_QUEST) &&
            (n.effect.p1 == 0 || n.effect.p1 == DIALOGUE_NONE))
            return fail(n.id);
        for (size_t c = 0; c < n.choice_count; ++c) {
            const DialogueChoice& ch = n.choices[c];
            if (!ch.text || !ch.text[0]) return fail(n.id);
            if (ch.vocab_count > DIALOGUE_MAX_TAGS) return fail(n.id);
            if (ch.grammar_count > DIALOGUE_MAX_TAGS) return fail(n.id);
            if (ch.next == DIALOGUE_NONE) continue; // terminal choice
            bool found = false;
            for (size_t k = 0; k < def.node_count; ++k) {
                if (def.nodes[k].id == ch.next) {
                    found = true;
                    break;
                }
            }
            if (!found) return fail(n.id); // dangling reference
        }
    }
    return true;
}

bool dialogue_validate_bank(const DialogueBank& bank, uint16_t* bad_def) {
    auto fail = [&](uint16_t d) {
        if (bad_def) *bad_def = d;
        return false;
    };
    if (!bank.defs && bank.count != 0) return fail(DIALOGUE_NONE);
    for (size_t i = 0; i < bank.count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (bank.defs[j].id == bank.defs[i].id) return fail(bank.defs[i].id);
        }
        uint16_t bad_node = 0;
        if (!dialogue_validate(bank.defs[i], &bad_node)) return fail(bank.defs[i].id);
    }
    return true;
}

const DialogueDef* dialogue_find(const DialogueBank& bank, uint16_t id) {
    if (!bank.defs) return nullptr;
    for (size_t i = 0; i < bank.count; ++i) {
        if (bank.defs[i].id == id) return &bank.defs[i];
    }
    return nullptr;
}

const DialogueNode* dialogue_find_node(const DialogueDef& def, uint16_t node) {
    if (!def.nodes) return nullptr;
    for (size_t i = 0; i < def.node_count; ++i) {
        if (def.nodes[i].id == node) return &def.nodes[i];
    }
    return nullptr;
}

static void arrive(DialogueSession& s, const DialogueNode* at) {
    s.node = at->id;
    // A choiceless node is terminal: arrival completes the dialogue.
    s.state = (at->choice_count == 0) ? uint8_t(DialogueState::COMPLETED)
                                      : uint8_t(DialogueState::ACTIVE);
}

bool dialogue_begin(DialogueSession& s, const DialogueBank& bank,
                    uint16_t dialogue, uint16_t npc) {
    if (npc == DIALOGUE_NONE) return false;
    const DialogueDef* def = dialogue_find(bank, dialogue);
    if (!def || def->node_count == 0) return false;
    s.dialogue = dialogue;
    s.npc = npc;
    arrive(s, &def->nodes[0]); // first node is the entry point
    return true;
}

bool dialogue_choose(DialogueSession& s, const DialogueBank& bank,
                     size_t choice) {
    if (s.state != uint8_t(DialogueState::ACTIVE)) return false;
    const DialogueDef* def = dialogue_find(bank, s.dialogue);
    if (!def) return false;
    const DialogueNode* at = dialogue_find_node(*def, s.node);
    if (!at || choice >= at->choice_count) return false;
    const uint16_t next = at->choices[choice].next;
    if (next == DIALOGUE_NONE) {
        s.state = uint8_t(DialogueState::COMPLETED);
        return true;
    }
    const DialogueNode* to = dialogue_find_node(*def, next);
    if (!to) return false; // validated content never hits this
    arrive(s, to);
    return true;
}

void dialogue_end(DialogueSession& s) {
    s.dialogue = DIALOGUE_NONE;
    s.node = DIALOGUE_NONE;
    s.npc = DIALOGUE_NONE;
    s.state = uint8_t(DialogueState::EMPTY);
}

bool dialogue_abort(DialogueSession& s) {
    if (s.state != uint8_t(DialogueState::ACTIVE)) return false;
    s.state = uint8_t(DialogueState::ABORTED);
    return true;
}

bool dialogue_selfcheck() {
    static const char t1[] = "Excuse me, can you help me?";
    static const char c1[] = "Yes.";
    static const char c2[] = "No.";
    static const char t2[] = "Thank you!";
    static const DialogueNode nodes[] = {
        {1, uint8_t(DialogueSpeaker::NPC), 2,
         {{c1, 2}, {c2, DIALOGUE_NONE}}, t1, DialogueLangMeta{}, {}, {}},
        {2, uint8_t(DialogueSpeaker::NPC), 0,
         {}, t2, DialogueLangMeta{}, {}, {}},
    };
    static const DialogueDef def{7, 1, 2, nodes};
    static const DialogueBank bank{&def, 1};
    if (!dialogue_validate_bank(bank, nullptr)) return false;
    if (dialogue_find(bank, 8) != nullptr) return false;

    DialogueSession s{};
    if (!dialogue_begin(s, bank, 7, 0x0100)) return false;
    if (s.state != uint8_t(DialogueState::ACTIVE) || s.node != 1) return false;
    if (dialogue_choose(s, bank, 5)) return false; // bad index
    if (!dialogue_choose(s, bank, 1)) return false; // "No." -> terminal
    if (s.state != uint8_t(DialogueState::COMPLETED)) return false;
    if (dialogue_choose(s, bank, 0)) return false; // completed: locked
    if (dialogue_abort(s)) return false;          // abort needs ACTIVE
    dialogue_end(s);
    if (s.state != uint8_t(DialogueState::EMPTY)) return false;

    if (!dialogue_begin(s, bank, 7, 0x0100)) return false;
    if (!dialogue_choose(s, bank, 0)) return false; // -> node 2, choiceless
    if (s.state != uint8_t(DialogueState::COMPLETED) || s.node != 2) return false;
    if (!dialogue_begin(s, bank, 7, 0x0100)) return false;
    if (!dialogue_abort(s)) return false;
    if (s.state != uint8_t(DialogueState::ABORTED)) return false;
    if (dialogue_begin(s, bank, 77, 0x0100)) return false; // unknown id
    if (dialogue_begin(s, bank, 7, DIALOGUE_NONE)) return false;
    return true;
}

} // namespace l3d
