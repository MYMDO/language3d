// Roadmap Phase 12 integration: the existing "Getting to the Station"
// quest becomes a language-learning scenario. Dialogue node entries record
// SEEN exposure, player choices record USED, item handoffs record SEEN —
// quest progression and language progress advance side by side.
#include "../engine/language.h"
#include "../engine/dialogue_quest.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

namespace l3d {
const DialogueBank content_dialogues();
const QuestBank content_quests();
const ItemBank content_items();
const VocabularyBank content_vocabulary();
}

int main() {
    const DialogueBank dbank = content_dialogues();
    const QuestBank qbank = content_quests();
    const ItemBank items = content_items();
    const VocabularyBank vbank = content_vocabulary();

    // Quest scenario linkage: topic 1 = transportation.
    const QuestDef* station = quest_find(qbank, 1);
    L3D_REQUIRE(station && station->topic == 1);

    // Every vocab tag referenced by the scenario content must resolve.
    for (size_t di = 0; di < dbank.count; ++di) {
        const DialogueDef& d = dbank.defs[di];
        for (size_t ni = 0; ni < d.node_count; ++ni) {
            const DialogueNode& n = d.nodes[ni];
            for (size_t vi = 0; vi < n.lang.vocab_count; ++vi) {
                L3D_REQUIRE(vocab_find(vbank, n.lang.vocab[vi]) != nullptr);
            }
        }
    }
    for (size_t i = 0; i < items.count; ++i) {
        for (size_t vi = 0; vi < items.defs[i].vocab_count; ++vi) {
            L3D_REQUIRE(vocab_find(vbank, items.defs[i].vocab[vi]) != nullptr);
        }
    }

    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    LanguageProfile<32> prof{};
    prof.init(pair);
    QuestLog<8> log{};
    log.init();
    PlayerState<8> player{};
    player.init(0x0100);
    Inventory<8> inv{};
    inv.init();
    player.bind_inventory(&inv);
    using OT = ObjectiveType;

    // --- Anna opens: entry node exposes greeting/help vocab ---
    DialogueSession s{};
    L3D_REQUIRE(dq_begin(s, dbank, log, qbank, items, player, 1, 0x0100, nullptr));
    const DialogueNode* n1 = dialogue_find_node(*dialogue_find(dbank, 1), 1);
    L3D_REQUIRE(lang_observe_dialogue(prof, vbank, *n1) == 2); // 101, 102
    L3D_REQUIRE(prof.progress_of(101)->exposures == 1);
    // Player asks ("Yes"): entering node 2 exposes station vocab + STARTs quest.
    DialogueStep st = dq_choose(s, dbank, player, log, qbank, items, prof, vbank, 0);
    L3D_REQUIRE(st.advanced);
    const DialogueNode* n2 = dialogue_find_node(*dialogue_find(dbank, 1), 2);
    L3D_REQUIRE(lang_observe_dialogue(prof, vbank, *n2) == 1); // 103
    // The player's choice counts as USE of the entered node's vocabulary.
    for (size_t i = 0; i < n2->lang.vocab_count; ++i)
        L3D_REQUIRE(prof.record(vbank, n2->lang.vocab[i], LanguageEvent::USED));
    L3D_REQUIRE(prof.progress_of(103)->uses == 1);
    L3D_REQUIRE(log.find(1)->state == uint8_t(QuestState::ACTIVE));

    // --- replaying the same opener is idempotent, never inflates ---
    L3D_REQUIRE(!prof.ensure_seen(vbank, 101));
    L3D_REQUIRE(prof.progress_of(101)->exposures == 1);

    // --- quest walk-through advances both tracks ---
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::TALK), 1, 0, 0) ==
                QuestEvent::OBJECTIVE_DONE);
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::REACH), 3, 0, 0) ==
                QuestEvent::OBJECTIVE_DONE);
    // Clerk handoff: ticket item exposes its vocabulary on receipt.
    L3D_REQUIRE(inv.add(items, 2, 1) == 0);
    const ItemDef* ticket = item_find(items, 2);
    L3D_REQUIRE(lang_observe_item(prof, vbank, *ticket) == 2); // 103, 203
    L3D_REQUIRE(prof.progress_of(203)->exposures == 1);
    L3D_REQUIRE(prof.progress_of(103)->exposures == 3); // seen, used, seen
    L3D_REQUIRE(mastery_of(*prof.progress_of(103)) == Mastery::LEARNING);
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::COLLECT), 0, 2, 1) ==
                QuestEvent::OBJECTIVE_DONE);
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::GIVE), 2, 2, 1) ==
                QuestEvent::QUEST_COMPLETED);
    L3D_REQUIRE(quest_claim(log, qbank, items, player, 1) == QuestEvent::CLAIMED);

    // --- scenario summary: learning state mirrors gameplay state ---
    L3D_REQUIRE(prof.progress_of(101) != nullptr); // greeted
    L3D_REQUIRE(prof.progress_of(203) != nullptr); // ticket handled
    L3D_REQUIRE(log.find(1)->state == uint8_t(QuestState::CLAIMED));

    std::printf("language-scenario OK\n");
    return 0;
}
