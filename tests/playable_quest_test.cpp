// Roadmap Phase 11B-3: first playable quest end-to-end (scripted game loop).
//
// Player finds Anna -> E -> dialogue (quest START effect) -> walks to the
// station district -> talks to the clerk (ticket effect) -> quest events ->
// COMPLETED -> claim with rewards. Every pool from Phases 2-11A takes part;
// the test itself is the temporary orchestrator until the game loop exists.
#include "../engine/npc_dispatch.h"
#include "../engine/interact.h"
#include "../engine/dialogue_quest.h"
#include "../engine/entity.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

namespace l3d {
const DialogueBank content_dialogues();
const QuestBank content_quests();
const ItemBank content_items();
}

static bool open_space(const Vec3&, const Vec3&) { return false; }

int main() {
    // --- world population ---
    EntityPool<8> entities{};
    entities.init();
    TransformPool<8> transforms{};
    NPCPool<8> npcs{};
    npcs.init();
    SchedulePool<8> sched{};
    sched.init();
    const DialogueBank dbank = content_dialogues();
    const QuestBank qbank = content_quests();
    const ItemBank items = content_items();
    L3D_REQUIRE(dbank.count == 2 && qbank.count == 1 && items.count == 4);

    const uint16_t e_player = entities.spawn(EntityKind::PLAYER);
    const uint16_t e_anna = entities.spawn(EntityKind::NPC);
    const uint16_t e_clerk = entities.spawn(EntityKind::NPC);
    Transform3 tp{};
    tp.pos = Vec3{Fx::from_int(1), Fx::from_int(1), Fx{}};
    tp.yaw = 0; // facing +X toward Anna at (2,1)
    transforms.attach(e_player, tp);
    Transform3 ta{};
    ta.pos = Vec3{Fx::from_int(2), Fx::from_int(1), Fx{}};
    transforms.attach(e_anna, ta);
    Transform3 tc{};
    tc.pos = Vec3{Fx::from_int(11), Fx::from_int(2), Fx{}};
    transforms.attach(e_clerk, tc);
    const uint16_t anna = npcs.spawn(e_anna, NPCArchetype::RESIDENT);
    const uint16_t clerk = npcs.spawn(e_clerk, NPCArchetype::CLERK);

    // Anna lives on a schedule: HOME(1) -> SHOP(2) at 08:00.
    Region3 regions[2]{};
    regions[0].box.mn = Vec3{};
    regions[0].box.mx = Vec3{Fx::from_int(2), Fx::from_int(2), Fx::from_int(2)};
    regions[0].contentTag = 1;
    regions[1].box.mn = Vec3{Fx::from_int(10), Fx::from_int(0), Fx{}};
    regions[1].box.mx = Vec3{Fx::from_int(12), Fx::from_int(2), Fx::from_int(2)};
    regions[1].contentTag = 2;
    const ScheduleEntry aday[] = {
        {0, 480, 1, uint8_t(SchedBehavior::REST)},
        {480, 1440, 2, uint8_t(SchedBehavior::WORK)},
    };
    L3D_REQUIRE(sched.set(anna, aday, 2));
    GameClock clock{};
    clock.minute = 100;

    PlayerState<8> player{};
    player.init(e_player);
    Inventory<8> inv{};
    inv.init();
    player.bind_inventory(&inv);
    QuestLog<8> log{};
    log.init();
    const Fx reach = Fx::from_int(3);

    // --- 1. approach Anna, press E ---
    L3D_REQUIRE(interact_target(npcs, transforms, tp, reach) == anna);
    InteractSession is{};
    L3D_REQUIRE(interact_try(is, anna, 1000) == InteractResult::STARTED);

    // --- 2. talk: quest START effect fires on "Where is the station?" ---
    DialogueSession ds{};
    L3D_REQUIRE(dq_begin(ds, dbank, log, qbank, items, player, 1, anna, nullptr));
    L3D_REQUIRE(dq_choose(ds, dbank, player, log, qbank, items, 0).advanced);
    const QuestRuntime* qr = log.find(1);
    L3D_REQUIRE(qr && qr->state == uint8_t(QuestState::ACTIVE)); // TALK auto-reported? no:
    // (TALK objective completes when the dialogue opens on Anna: dq_begin
    // reported TALK(anna_tag=1) — but the quest was NOT_STARTED then, so the
    // report was IGNORED. The player must greet again now that it is ACTIVE.)
    L3D_REQUIRE(qr->objective_idx == 0);
    L3D_REQUIRE(interact_end(is, 2000) == InteractResult::ENDED);

    // --- 3. greet again: TALK objective completes via fan-out ---
    L3D_REQUIRE(interact_try(is, anna, 3000) == InteractResult::STARTED);
    L3D_REQUIRE(dq_begin(ds, dbank, log, qbank, items, player, 1, anna, nullptr));
    qr = log.find(1);
    L3D_REQUIRE(qr->objective_idx == 1); // TALK done
    L3D_REQUIRE(interact_end(is, 4000) == InteractResult::ENDED);
    dialogue_end(ds);

    // --- 4. morning comes: Anna walks to SHOP while the player travels ---
    clock.minute = 480;
    for (int i = 0; i < 12; ++i) {
        L3D_REQUIRE(npc_dispatch_step(npcs, transforms, sched, regions, 2,
                                      anna, clock, 500, Fx::from_int(2),
                                      open_space) == DispatchResult::OK);
    }
    L3D_REQUIRE(npcs.get(anna)->state == uint8_t(NPCState::ARRIVED));
    // Player walks (teleport stands in for the future controller) to station.
    tp.pos = Vec3{Fx::from_int(11), Fx::from_int(1), Fx{}};
    tp.yaw = 0x4000; // facing +Y toward the clerk at (11,2)
    transforms.attach(e_player, tp);
    using OT = ObjectiveType;
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::REACH), 3, 0, 0) ==
                QuestEvent::OBJECTIVE_DONE);

    // --- 5. clerk interaction: ticket effect, then COLLECT + GIVE ---
    L3D_REQUIRE(interact_target(npcs, transforms, tp, reach) == clerk);
    L3D_REQUIRE(interact_try(is, clerk, 5000) == InteractResult::STARTED);
    L3D_REQUIRE(dq_begin(ds, dbank, log, qbank, items, player, 2, clerk, nullptr));
    DialogueStep st = dq_choose(ds, dbank, player, log, qbank, items, 0);
    L3D_REQUIRE(st.advanced && st.effect.kind == uint8_t(EffectApply::APPLIED));
    L3D_REQUIRE(inv.has(2, 1)); // ticket in hand
    L3D_REQUIRE(interact_end(is, 6000) == InteractResult::ENDED);
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::COLLECT), 0, 2, 1) ==
                QuestEvent::OBJECTIVE_DONE);
    L3D_REQUIRE(quest_report(log, qbank, player, 1, uint8_t(OT::GIVE), 2, 2, 1) ==
                QuestEvent::QUEST_COMPLETED);

    // --- 6. claim: atomic rewards (+100 XP, flag 5) ---
    L3D_REQUIRE(quest_claim(log, qbank, items, player, 1) == QuestEvent::CLAIMED);
    L3D_REQUIRE(player.xp == 100 && player.has_flag(5));
    L3D_REQUIRE(inv.has(2, 1)); // GIVE checked possession, 11B keeps the item

    std::printf("playable-quest OK\n");
    return 0;
}
