// Roadmap Phase 17 tests: Scenario B ("An Apple for Anna") driven through
// the shared Scenario runtime — no scenario-specific orchestration code.
#include "../sdl2/scenario.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace l3d;

static Transform3 tr_at(int x, int y, uint16_t yaw = 0) {
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(x) + Fx::from_float(0.5f),
                 Fx::from_int(y) + Fx::from_float(0.5f), Fx{}};
    t.yaw = yaw;
    return t;
}

static bool open_solid(void*, int32_t, int32_t) { return false; }

// One full shopkeeper dialogue (dlg 4): open, request, thanks, dismiss.
static void play_shop(Scenario& sc, uint32_t& now) {
    now += 600;
    sc.setPlayer(tr_at(17, 18, 0x4000));
    L3D_REQUIRE(sc.pressE(now));
    L3D_REQUIRE(sc.dialogueOpen());
    L3D_REQUIRE(sc.pressAnswer(1));
    L3D_REQUIRE(sc.pressAnswer(1));
    now += 600;
    L3D_REQUIRE(sc.pressE(now)); // dismiss terminal panel
    L3D_REQUIRE(!sc.dialogueOpen());
}

int main() {
    Scenario sc{};
    const DialogueBank dbank = content_dialogues();
    const QuestBank qbank = content_quests();
    const ItemBank ibank = content_items();
    const VocabularyBank vbank = content_vocabulary();
    const ScenarioDef* def = findScenarioDef("shop");
    L3D_REQUIRE(def != nullptr);
    L3D_REQUIRE(def->quest == 2);
    L3D_REQUIRE(def->npcCount == 2);
    L3D_REQUIRE(sc.init(&dbank, &qbank, &ibank, &vbank, def));
    L3D_REQUIRE(!sc.init(nullptr, &qbank, &ibank, &vbank, def));
    uint32_t now = 1000;

    // --- unknown scenario id resolves to nothing ---
    L3D_REQUIRE(findScenarioDef("bakery") == nullptr);
    L3D_REQUIRE(findScenarioDef(nullptr) == nullptr);
    L3D_REQUIRE(scenarioDefCount() == 2);
    L3D_REQUIRE(scenarioDefAt(0) != nullptr && scenarioDefAt(1) != nullptr);
    L3D_REQUIRE(scenarioDefAt(2) == nullptr);

    // --- variant: fallback greeting before any mastery ---
    L3D_REQUIRE(sc.dialogueFor(3) == 4);
    L3D_REQUIRE(std::strcmp(sc.objectiveText(),
                            "Explore: find Anna (walk to the NPC, press E)") == 0);

    // --- E with nobody near is not consumed ---
    sc.setPlayer(tr_at(4, 4));
    L3D_REQUIRE(!sc.pressE(now += 100));
    L3D_REQUIRE(!sc.pressAnswer(1));
    L3D_REQUIRE(std::strcmp(sc.promptText(), "") == 0);

    // --- approach the shopkeeper: prompt names him, E opens dialogue 4 ---
    sc.setPlayer(tr_at(17, 18, 0x4000));
    L3D_REQUIRE(std::strcmp(sc.promptText(), "Shopkeeper nearby - press E") == 0);
    L3D_REQUIRE(sc.pressE(now += 100));
    L3D_REQUIRE(sc.dialogueOpen());

    // --- talk through: quest START effect on node 2 ---
    L3D_REQUIRE(sc.pressAnswer(1)); // request -> node 2
    const QuestRuntime* qr = sc.quests().find(2);
    L3D_REQUIRE(qr && qr->state == uint8_t(QuestState::ACTIVE));
    L3D_REQUIRE(std::strcmp(sc.objectiveText(), "Quest: talk to Shopkeeper (E)") == 0);
    // Wrong answer holds the session open (shared INCORRECT machinery).
    L3D_REQUIRE(sc.pressE(now += 100)); // E during ACTIVE dialogue: held
    L3D_REQUIRE(sc.dialogueOpen());

    // --- walking away aborts through the shared machinery ---
    sc.setPlayer(tr_at(4, 4));
    sc.tick(16, now += 16, open_solid, nullptr);
    L3D_REQUIRE(!sc.dialogueOpen());

    // --- finish the dialogue: apple handed over, quest completes TALK ---
    sc.setPlayer(tr_at(17, 18, 0x4000));
    L3D_REQUIRE(sc.pressE(now += 600));
    L3D_REQUIRE(sc.dialogueOpen());
    L3D_REQUIRE(sc.pressAnswer(1)); // request -> node 2 again
    L3D_REQUIRE(sc.pressAnswer(1)); // thanks -> terminal node 4
    L3D_REQUIRE(sc.player().inventory->has(1, 1)); // apple received
    L3D_REQUIRE(sc.pressE(now += 100)); // dismiss
    L3D_REQUIRE(!sc.dialogueOpen());
    // Greet again: TALK fan-out completes objective 0.
    L3D_REQUIRE(sc.pressE(now += 600));
    L3D_REQUIRE(sc.dialogueOpen());
    qr = sc.quests().find(2);
    L3D_REQUIRE(qr->objective_idx == 1);
    // COLLECT auto-reports from the shared pump (apple already held).
    sc.tick(16, now += 16, open_solid, nullptr);
    qr = sc.quests().find(2);
    L3D_REQUIRE(qr->objective_idx == 2);

    // --- carry the apple to Anna: close the greeting first, then E ---
    // (E during ACTIVE dialogue is held by shared machinery, as in A.)
    sc.setPlayer(tr_at(4, 4));
    sc.tick(16, now += 16, open_solid, nullptr); // abort greeting
    L3D_REQUIRE(!sc.dialogueOpen());
    // (Anna dispatched toward MARKET; face her from the west.)
    sc.setPlayer(tr_at(17, 18));
    sc.tick(16, now += 16, open_solid, nullptr);
    L3D_REQUIRE(sc.pressE(now += 600)); // GIVE branch: apple handed over
    L3D_REQUIRE(!sc.dialogueOpen());
    qr = sc.quests().find(2);
    L3D_REQUIRE(qr->state == uint8_t(QuestState::CLAIMED)); // auto-claimed
    L3D_REQUIRE(sc.player().xp == 50);
    L3D_REQUIRE(sc.player().has_flag(6));
    L3D_REQUIRE(std::strcmp(sc.objectiveText(), "An Apple for Anna claimed. +50 XP") == 0);

    // --- save / load round-trip through a real file ---
    L3D_REQUIRE(sc.saveGame("l3d_shop_test.save"));
    L3D_REQUIRE(sc.loadGame("l3d_shop_test.save"));
    qr = sc.quests().find(2);
    L3D_REQUIRE(qr && qr->state == uint8_t(QuestState::CLAIMED));
    L3D_REQUIRE(sc.player().xp == 50);
    L3D_REQUIRE(!sc.loadGame("l3d_shop_nope.save")); // missing file

    // --- closed learning loop: two more shop talks -> FAMILIAR -> variant ---
    play_shop(sc, now);
    play_shop(sc, now);
    // apple: USED, CORRECT x3 total -> FAMILIAR
    L3D_REQUIRE(sc.dialogueFor(3) == 5);

    // --- reload keeps the new greeting: persistence affects next play ---
    L3D_REQUIRE(sc.saveGame("l3d_shop_test.save"));
    L3D_REQUIRE(sc.loadGame("l3d_shop_test.save"));
    L3D_REQUIRE(sc.dialogueFor(3) == 5);
    std::remove("l3d_shop_test.save"); // do not litter the working tree

    // --- panel rendering draws real pixels into a live framebuffer ---
    {
        static std::vector<uint8_t> big;
        big.assign(size_t(1920) * 1080, 0);
        sc.setPlayer(tr_at(17, 18, 0x4000));
        now += 600;
        L3D_REQUIRE(sc.pressE(now)); // familiar greeting (dialogue 5)
        sc.renderPanel(big.data(), 1920, 1080, 1920);
        size_t panel_px = 0, hud_px = 0;
        for (size_t y = size_t(1080) - 160; y < 1080; ++y)
            for (size_t x = 0; x < 1920; ++x)
                if (big[y * 1920 + x]) ++panel_px;
        for (size_t y = 0; y < 32; ++y)
            for (size_t x = 0; x < 1920; ++x)
                if (big[y * 1920 + x]) ++hud_px;
        L3D_REQUIRE(panel_px > 2000); // box + speaker + text + choices
        L3D_REQUIRE(hud_px > 20);     // objective line
    }

    std::printf("shop slice OK\n");
    return 0;
}
