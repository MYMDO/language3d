// Roadmap Phase 8 tests: generated content bank, traversal, choices,
// lifecycle, validation, NPC handoff. The generated pack exercises the
// real Anna scenario; hand-built defs cover the invalid cases.
#include "../engine/dialogue.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>

using namespace l3d;

namespace l3d {
const DialogueBank content_dialogues(); // generated_dialogue.cpp
}

int main() {
    // --- generated bank validates and matches the authored scenario ---
    const DialogueBank bank = content_dialogues();
    L3D_REQUIRE(bank.count == 3); // Anna station + clerk ticket + Anna familiar
    uint16_t bad = 0;
    L3D_REQUIRE(dialogue_validate_bank(bank, &bad));
    const DialogueDef* anna = dialogue_find(bank, 1);
    L3D_REQUIRE(anna && anna->npc_tag == 1 && anna->node_count == 5);
    const DialogueDef* clerk = dialogue_find(bank, 2);
    L3D_REQUIRE(clerk && clerk->npc_tag == 2 && clerk->node_count == 3);
    const DialogueNode* k2 = dialogue_find_node(*clerk, 2);
    L3D_REQUIRE(k2 && k2->choice_count == 0); // terminal handoff
    L3D_REQUIRE(k2->effect.kind == uint8_t(DialogueEffectKind::GIVE_ITEM));
    L3D_REQUIRE(k2->effect.p1 == 2 && k2->effect.p2 == 1);
    const DialogueNode* n1 = dialogue_find_node(*anna, 1);
    L3D_REQUIRE(n1 && n1->choice_count == 2);
    L3D_REQUIRE(std::strcmp(n1->text, "Excuse me, can you help me?") == 0);
    // Language metadata rides along, inert for gameplay.
    L3D_REQUIRE(n1->lang.lang == uint8_t(DialogueLang::EN));
    L3D_REQUIRE(n1->lang.cefr == uint8_t(DialogueCEFR::A2));
    L3D_REQUIRE(n1->lang.vocab_count == 2 && n1->lang.vocab[0] == 101);
    L3D_REQUIRE(n1->lang.grammar_count == 1 && n1->lang.grammar[0] == 13);

    // --- full traversal: polite path 1 -> 2 -> 4 ---
    DialogueSession s{};
    L3D_REQUIRE(dialogue_begin(s, bank, 1, 0x0100)); // NPC handoff stored
    L3D_REQUIRE(s.npc == 0x0100 && s.node == 1);
    L3D_REQUIRE(s.state == uint8_t(DialogueState::ACTIVE));
    L3D_REQUIRE(dialogue_choose(s, bank, 0));
    L3D_REQUIRE(s.node == 2 && s.state == uint8_t(DialogueState::ACTIVE));
    L3D_REQUIRE(dialogue_choose(s, bank, 0));
    L3D_REQUIRE(s.node == 4 && s.state == uint8_t(DialogueState::COMPLETED));
    L3D_REQUIRE(!dialogue_choose(s, bank, 0)); // completed: locked
    dialogue_end(s);
    L3D_REQUIRE(s.state == uint8_t(DialogueState::EMPTY));

    // --- second path 1 -> 3 terminates on arrival ---
    L3D_REQUIRE(dialogue_begin(s, bank, 1, 0x0100));
    L3D_REQUIRE(dialogue_choose(s, bank, 1));
    L3D_REQUIRE(s.node == 3 && s.state == uint8_t(DialogueState::COMPLETED));

    // --- abort path: walk away mid-dialogue ---
    L3D_REQUIRE(dialogue_begin(s, bank, 1, 0x0100));
    L3D_REQUIRE(dialogue_choose(s, bank, 0));
    L3D_REQUIRE(dialogue_abort(s));
    L3D_REQUIRE(s.state == uint8_t(DialogueState::ABORTED));
    L3D_REQUIRE(!dialogue_choose(s, bank, 0)); // aborted: locked
    L3D_REQUIRE(!dialogue_abort(s));           // abort needs ACTIVE
    dialogue_end(s);

    // --- bad choice index rejected ---
    L3D_REQUIRE(dialogue_begin(s, bank, 1, 0x0100));
    L3D_REQUIRE(!dialogue_choose(s, bank, 7));
    L3D_REQUIRE(s.node == 1 && s.state == uint8_t(DialogueState::ACTIVE));
    dialogue_end(s);

    // --- NPC binding rules ---
    L3D_REQUIRE(!dialogue_begin(s, bank, 1, DIALOGUE_NONE)); // sentinel
    L3D_REQUIRE(!dialogue_begin(s, bank, 777, 0x0100));      // unknown id
    // Stale NPC liveness is the owner's job (documented discipline):
    // the session stores the handoff opaquely.
    L3D_REQUIRE(dialogue_begin(s, bank, 1, 0x02FF));
    L3D_REQUIRE(s.npc == 0x02FF);
    dialogue_end(s);

    // --- determinism: same choice sequence, same states ---
    DialogueSession a{}, b{};
    dialogue_begin(a, bank, 1, 0x0100);
    dialogue_begin(b, bank, 1, 0x0100);
    dialogue_choose(a, bank, 0);
    dialogue_choose(b, bank, 0);
    L3D_REQUIRE(a.node == b.node && a.state == b.state);

    // --- runtime validator catches hand-built breakage ---
    {
        static const char t[] = "Hi";
        static const char c[] = "Go";
        static const DialogueNode dangling[] = {
            {1, 0, 1, {{c, 99}}, t, DialogueLangMeta{}, {}, {}},
        };
        static const DialogueDef bad_def{9, 1, 1, dangling};
        uint16_t bad_node = 0;
        L3D_REQUIRE(!dialogue_validate(bad_def, &bad_node) && bad_node == 1);
        static const DialogueNode dup[] = {
            {1, 0, 0, {}, t, DialogueLangMeta{}, {}, {}},
            {1, 0, 0, {}, t, DialogueLangMeta{}, {}, {}},
        };
        static const DialogueDef dup_def{10, 1, 2, dup};
        L3D_REQUIRE(!dialogue_validate(dup_def, nullptr));
        static const DialogueDef empty_def{11, 1, 0, nullptr};
        L3D_REQUIRE(!dialogue_validate(empty_def, nullptr));
        static const DialogueDef bank_defs[] = {*anna, *anna};
        const DialogueBank dup_bank{bank_defs, 2};
        uint16_t bad_id = 0;
        L3D_REQUIRE(!dialogue_validate_bank(dup_bank, &bad_id) && bad_id == 1);
    }

    L3D_REQUIRE(dialogue_selfcheck());

    std::printf("dialogue OK\n");
    return 0;
}
