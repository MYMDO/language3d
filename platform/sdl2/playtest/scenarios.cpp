// Phase 16B: the 8 required gameplay scenarios. Each drives REAL public
// APIs (engines, Scenario, save codec) with deterministic fixtures — no
// engine changes, no test-only shortcuts around gameplay paths.
#include "playtest.h"

#include "../../engine/world3.h"
#include "../../engine/entity.h"
#include "../../engine/npc.h"
#include "../../engine/schedule.h"
#include "../../engine/interact.h"
#include "../../engine/dialogue_quest.h"
#include "../../engine/language.h"
#include "../../engine/quest.h"
#include "../../engine/item.h"
#include "../../engine/player.h"
#include "../../engine/save.h"
#include "../scenario.h"

#include <cstdio>
#include <cstring>

namespace l3d {
namespace playtest {

namespace {

// ---- shared fixtures (caller-owned, bounded, deterministic) ----

static bool solid_maze(void*, int32_t x, int32_t y) {
    if (x < 0 || y < 0 || x >= 5 || y >= 5) return true;
    if (x == 3 && y <= 3) return true; // wall column with a gap at y==4
    return false;
}

static bool solid_cells(int32_t x, int32_t y) {
    return solid_maze(nullptr, x, y);
}

static bool open_space(void*, int32_t, int32_t) { return false; }

static Transform3 tr_at(int x, int y, uint16_t yaw = 0) {
    Transform3 t{};
    t.pos = Vec3{Fx::from_int(x) + Fx::from_float(0.5f),
                 Fx::from_int(y) + Fx::from_float(0.5f), Fx{}};
    t.yaw = yaw;
    return t;
}

static void snap(AssertCtx&, char* state_text, size_t cap, const char* s) {
    size_t i = 0;
    while (s[i] && i + 1 < cap) {
        state_text[i] = s[i];
        ++i;
    }
    state_text[i] = '\0';
}

static void snap_u32(char* state_text, size_t cap, const char* key, uint32_t v) {
    std::snprintf(state_text, cap, "%s%u", key, v);
}

} // namespace

// ---- Scenario 1: basic movement (World3 walk_move, real collision) ----
static bool sc_basic_movement(AssertCtx& ctx, EventLog& log, char* state,
                              size_t cap, uint32_t) {
    log.log(EventType::GAME_RESET, 0, 0, 0);
    static const int8_t flat[25] = {};
    HeightField f{};
    f.floorQ = flat;
    f.ceilQ = nullptr;
    f.w = 5;
    f.h = 5;
    Vec3 pos{tr_at(1, 1).pos};
    MoveQuery q{};
    log.log(EventType::PLAYER_MOVE, 1, 1, 0);
    walk_move(f, solid_cells, pos, q, Vec2{Fx::from_int(1), Fx::from_int(0)});
    if (!assert_true(ctx, pos.x.raw == tr_at(2, 1).pos.x.raw,
                     "step east onto (2,1)"))
        return false;
    walk_move(f, solid_cells, pos, q, Vec2{Fx::from_int(1), Fx::from_int(0)});
    if (!assert_true(ctx, pos.x.raw == tr_at(2, 1).pos.x.raw,
                     "wall at x==3 blocks"))
        return false;
    walk_move(f, solid_cells, pos, q, Vec2{Fx::from_int(0), Fx::from_int(1)});
    if (!assert_true(ctx, pos.y.raw == tr_at(2, 2).pos.y.raw,
                     "step south onto (2,2)"))
        return false;
    // Stairs: 1/8-unit ramp along row 4 climbs exactly 0.5 over 4 cells.
    static const int8_t ramp_rows[25] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
    };
    HeightField ramp{};
    ramp.floorQ = ramp_rows;
    ramp.ceilQ = nullptr;
    ramp.w = 5;
    ramp.h = 5;
    Vec3 sp{tr_at(0, 4).pos};
    for (int i = 0; i < 4; ++i)
        walk_move(ramp, solid_cells, sp, q,
                  Vec2{Fx::from_int(1), Fx::from_int(0)});
    if (!assert_true(ctx, sp.x.raw == tr_at(4, 4).pos.x.raw,
                     "ramp reaches x==4"))
        return false;
    if (!assert_true(ctx, sp.z.raw == Fx::from_float(0.5f).raw,
                     "ramp climbs to z==0.5"))
        return false;
    char buf[64]{};
    snap_u32(buf, sizeof(buf), "pos=", uint32_t(pos.x.raw));
    snap(ctx, state, cap, buf);
    return true;
}

// ---- Scenario 2: NPC interaction (proximity + facing + E) ----
static bool sc_npc_interaction(AssertCtx& ctx, EventLog& log, char* state,
                               size_t cap, uint32_t) {
    EntityPool<8> entities{};
    TransformPool<8> transforms{};
    NPCPool<8> npcs{};
    entities.init();
    npcs.init();
    const uint16_t e = entities.spawn(EntityKind::NPC);
    Transform3 anna = tr_at(2, 1);
    transforms.attach(e, anna);
    const uint16_t anna_id = npcs.spawn(e, NPCArchetype::RESIDENT);
    if (!assert_true(ctx, anna_id != L3D_NPC_INVALID, "anna spawns"))
        return false;

    Transform3 player = tr_at(1, 1, 0); // facing +X toward Anna
    uint16_t target =
        interact_target(npcs, transforms, player, Fx::from_int(3));
    if (!assert_true(ctx, target == anna_id, "nearest in-front NPC targeted"))
        return false;
    log.log(EventType::NPC_INTERACT, target, 0, 0);
    InteractSession s{};
    if (!assert_true(ctx,
                     interact_try(s, target, 1000) == InteractResult::STARTED,
                     "E opens session"))
        return false;
    if (!assert_true(ctx, interact_end(s, 1100) == InteractResult::ENDED,
                     "session ends"))
        return false;
    // Facing away: no target even in radius.
    player.yaw = 0x8000;
    target = interact_target(npcs, transforms, player, Fx::from_int(3));
    if (!assert_true(ctx, target == L3D_INTERACT_NONE,
                     "facing away finds nobody"))
        return false;
    snap(ctx, state, cap, "target=anna,session=open-close");
    return true;
}

// ---- Scenario 3: dialogue evaluation (CORRECT/PARTIAL/INCORRECT) ----
static bool sc_dialogue_evaluation(AssertCtx& ctx, EventLog& log, char* state,
                                   size_t cap, uint32_t) {
    // Hand dialogue: node 1 expects intent 10 (primary) / 20 (secondary).
    // CORRECT is deliberately at choice index 1 (position must not matter).
    static const char t1[] = "Right?";
    static const char a0[] = "Maybe.";
    static const char a1[] = "Exactly.";
    static const char a2[] = "Nope.";
    static const DialogueNode enodes[] = {
        {1, 0, 3,
         {{a0, 2, 20, {}, 0, {}, 0},
          {a1, 2, 10, {}, 0, {}, 0},
          {a2, 2, 30, {}, 0, {}, 0}},
         t1, DialogueLangMeta{}, {}, {}, 10, 20},
        {2, 0, 0, {}, t1, DialogueLangMeta{}, {}, {}},
    };
    static const DialogueDef edef{11, 7, 2, enodes};
    static const DialogueBank ebank{&edef, 1};
    static const QuestBank qbank{nullptr, 0};
    static const ItemBank ibank{nullptr, 0};
    static const VocabularyBank vbank{nullptr, 0};
    QuestLog<4> qlog{};
    qlog.init();
    PlayerState<4> p{};
    p.init(0x0100);
    LanguageProfile<8> lang{};
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    lang.init(pair);

    DialogueSession s{};
    if (!assert_true(ctx,
                     dq_begin(s, ebank, qlog, qbank, ibank, p, 11, 0x0100, nullptr),
                     "evaluated dialogue opens"))
        return false;
    log.log(EventType::DIALOGUE_START, 11, 0, 0);
    DialogueStep st =
        dq_choose(s, ebank, p, qlog, qbank, ibank, lang, vbank, 1);
    log.log(EventType::DIALOGUE_CHOICE, 1, 0, 0);
    log.log(EventType::RESPONSE_EVALUATED, uint8_t(ResponseVerdict::CORRECT), 0, 0);
    if (!assert_true(ctx, st.advanced, "correct advances")) return false;
    if (!assert_verdict(ctx, st.verdict, uint8_t(ResponseVerdict::CORRECT),
                        "verdict CORRECT at shuffled index"))
        return false;
    // Reopen: secondary intent -> PARTIAL, still advances.
    if (!assert_true(ctx,
                     dq_begin(s, ebank, qlog, qbank, ibank, p, 11, 0x0100, nullptr),
                     "dialogue reopens"))
        return false;
    st = dq_choose(s, ebank, p, qlog, qbank, ibank, lang, vbank, 0);
    log.log(EventType::RESPONSE_EVALUATED, uint8_t(ResponseVerdict::PARTIAL), 0, 0);
    if (!assert_true(ctx, st.advanced, "partial advances")) return false;
    if (!assert_verdict(ctx, st.verdict, uint8_t(ResponseVerdict::PARTIAL),
                        "verdict PARTIAL"))
        return false;
    // Reopen: unrelated intent -> INCORRECT, session holds open.
    if (!assert_true(ctx,
                     dq_begin(s, ebank, qlog, qbank, ibank, p, 11, 0x0100, nullptr),
                     "dialogue reopens again"))
        return false;
    st = dq_choose(s, ebank, p, qlog, qbank, ibank, lang, vbank, 2);
    log.log(EventType::RESPONSE_EVALUATED, uint8_t(ResponseVerdict::INCORRECT), 0, 0);
    if (!assert_true(ctx, !st.advanced, "incorrect holds session")) return false;
    if (!assert_verdict(ctx, st.verdict, uint8_t(ResponseVerdict::INCORRECT),
                        "verdict INCORRECT"))
        return false;
    if (!assert_dialogue_state(ctx, s.state, uint8_t(DialogueState::ACTIVE),
                               "session still active after incorrect"))
        return false;
    // Legacy node without expectations: verdict NONE, plain advance.
    static const DialogueNode lnodes[] = {
        {1, 0, 1, {{a1, 2}}, t1, DialogueLangMeta{}, {}, {}},
        {2, 0, 0, {}, t1, DialogueLangMeta{}, {}, {}},
    };
    static const DialogueDef ldef{12, 7, 2, lnodes};
    static const DialogueBank lbank{&ldef, 1};
    if (!assert_true(ctx,
                     dq_begin(s, lbank, qlog, qbank, ibank, p, 12, 0x0100, nullptr),
                     "legacy dialogue opens"))
        return false;
    st = dq_choose(s, lbank, p, qlog, qbank, ibank, lang, vbank, 0);
    if (!assert_true(ctx, st.advanced, "legacy advances")) return false;
    if (!assert_verdict(ctx, st.verdict, uint8_t(ResponseVerdict::NONE),
                        "legacy verdict NONE"))
        return false;
    snap(ctx, state, cap, "verdicts=correct,partial,incorrect,none");
    return true;
}

// ---- Scenario 4: adaptive dialogue (mastery-gated variants) ----
static bool sc_adaptive_dialogue(AssertCtx& ctx, EventLog& log, char* state,
                                 size_t cap, uint32_t) {
    // Group 5: fallback (21), LEARNING tier (22 on word 301),
    // FAMILIAR tier (23 on word 301). Untiered dialogue 24 ignored.
    static const char t[] = "V";
    static const DialogueNode vn[] = {
        {1, 0, 0, {}, t, DialogueLangMeta{}, {}, {}}};
    static const DialogueDef vdefs[] = {
        {21, 7, 1, vn, 5, 0, 0},
        {22, 7, 1, vn, 5, 301, 2},
        {23, 7, 1, vn, 5, 301, 3},
        {24, 7, 1, vn, 0, 0, 0},
    };
    static const DialogueBank vbank2{vdefs, 4};
    static const VocabularyEntry wentries[] = {
        {301, "yes", uint8_t(DialogueLang::EN), uint8_t(WordPOS::PHRASE),
         uint8_t(DialogueCEFR::A1), "agree"},
    };
    static const VocabularyBank wbank{wentries, 1};
    LanguageProfile<8> prof{};
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    prof.init(pair);

    uint16_t v = dq_select_variant(prof, vbank2, 5);
    log.log(EventType::VARIANT_SELECTED, v, 0, 0);
    if (!assert_eq_u32(ctx, v, 21, "fresh profile gets fallback")) return false;
    if (!assert_eq_u32(ctx, dq_select_variant(prof, vbank2, 6), DIALOGUE_NONE,
                       "unknown group selects nothing"))
        return false;
    if (!assert_eq_u32(ctx, dq_select_variant(prof, vbank2, 0), DIALOGUE_NONE,
                       "legacy group 0 selects nothing"))
        return false;
    for (int i = 0; i < 3; ++i)
        if (!prof.record(wbank, 301, LanguageEvent::SEEN)) return false;
    v = dq_select_variant(prof, vbank2, 5);
    log.log(EventType::VARIANT_SELECTED, v, 1, 0);
    if (!assert_eq_u32(ctx, v, 22, "exact LEARNING threshold promotes"))
        return false;
    for (int i = 0; i < 3; ++i)
        if (!prof.record(wbank, 301, LanguageEvent::CORRECT)) return false;
    v = dq_select_variant(prof, vbank2, 5);
    log.log(EventType::VARIANT_SELECTED, v, 2, 0);
    if (!assert_eq_u32(ctx, v, 23, "FAMILIAR promotes to top tier"))
        return false;
    // Tie-break: duplicate tier resolves to the lowest dialogue id.
    static const DialogueDef tdefs[] = {
        {31, 7, 1, vn, 9, 301, 2},
        {30, 7, 1, vn, 9, 301, 2},
    };
    static const DialogueBank tbank{tdefs, 2};
    if (!assert_eq_u32(ctx, dq_select_variant(prof, tbank, 9), 30,
                       "tie breaks to lowest id"))
        return false;
    // Missing variant: gated-only group with nothing met.
    static const DialogueDef mdefs[] = {
        {41, 7, 1, vn, 8, 302, 4},
    };
    static const DialogueBank mbank{mdefs, 1};
    if (!assert_eq_u32(ctx, dq_select_variant(prof, mbank, 8), DIALOGUE_NONE,
                       "missing variant selects nothing"))
        return false;
    char buf[64]{};
    snap_u32(buf, sizeof(buf), "variant=", v);
    snap(ctx, state, cap, buf);
    return true;
}

// ---- Scenario 5: quest flow (real station quest, real banks) ----
static bool sc_quest_flow(AssertCtx& ctx, EventLog& log, char* state,
                          size_t cap, uint32_t) {
    const QuestBank qbank = content_quests();
    const ItemBank items = content_items();
    QuestLog<8> qlog{};
    qlog.init();
    PlayerState<8> player{};
    player.init(0x0100);
    Inventory<8> inv{};
    inv.init();
    player.bind_inventory(&inv);

    if (!assert_true(ctx, quest_start(qlog, qbank, 1) == QuestEvent::STARTED,
                     "station quest starts"))
        return false;
    log.log(EventType::QUEST_STATE_CHANGED, 1,
            uint8_t(QuestState::ACTIVE), 0);
    using OT = ObjectiveType;
    if (!assert_true(ctx,
                     quest_report(qlog, qbank, player, 1, uint8_t(OT::TALK), 1, 0, 0) ==
                         QuestEvent::OBJECTIVE_DONE,
                     "TALK Anna completes objective 0"))
        return false;
    log.log(EventType::QUEST_OBJECTIVE_CHANGED, 1, 1, 0);
    if (!assert_true(ctx,
                     quest_report(qlog, qbank, player, 1, uint8_t(OT::REACH), 3, 0, 0) ==
                         QuestEvent::OBJECTIVE_DONE,
                     "REACH station completes objective 1"))
        return false;
    // Wrong order would be IGNORED; here COLLECT is current.
    if (!assert_true(ctx,
                     quest_report(qlog, qbank, player, 1, uint8_t(OT::COLLECT), 0, 2, 1) ==
                         QuestEvent::OBJECTIVE_DONE,
                     "COLLECT ticket completes objective 2"))
        return false;
    if (!assert_true(ctx, inv.add(items, 2, 1) == 0, "ticket fits inventory"))
        return false;
    log.log(EventType::ITEM_ADDED, 2, 1, 0);
    if (!assert_true(ctx,
                     quest_report(qlog, qbank, player, 1, uint8_t(OT::GIVE), 2, 2, 1) ==
                         QuestEvent::QUEST_COMPLETED,
                     "GIVE ticket completes quest"))
        return false;
    log.log(EventType::QUEST_STATE_CHANGED, 1,
            uint8_t(QuestState::COMPLETED), 0);
    if (!assert_true(ctx,
                     quest_claim(qlog, qbank, items, player, 1) == QuestEvent::CLAIMED,
                     "claim commits rewards"))
        return false;
    log.log(EventType::QUEST_STATE_CHANGED, 1,
            uint8_t(QuestState::CLAIMED), 0);
    const QuestRuntime* r = qlog.find(1);
    if (!assert_true(ctx, r && r->state == uint8_t(QuestState::CLAIMED),
                     "quest claimed"))
        return false;
    if (!assert_true(ctx, player.xp == 100, "reward +100 XP")) return false;
    snap(ctx, state, cap, "quest=claimed,xp=100");
    return true;
}

// ---- Scenario 6: inventory (real starter items) ----
static bool sc_inventory(AssertCtx& ctx, EventLog& log, char* state,
                         size_t cap, uint32_t) {
    const ItemBank bank = content_items();
    Inventory<8> inv{};
    inv.init();
    if (!assert_true(ctx, inv.add(bank, 1, 7) == 0, "7 apples fit"))
        return false;
    log.log(EventType::ITEM_ADDED, 1, 7, 0);
    if (!assert_true(ctx, inv.add(bank, 1, 8) == 0, "8 more stack to 10+5"))
        return false;
    if (!assert_true(ctx, inv.count_of(1) == 15, "15 apples counted"))
        return false;
    if (!assert_true(ctx, inv.add(bank, 2, 1) == 0, "ticket fits"))
        return false;
    if (!assert_true(ctx, inv.add(bank, 3, 1) == 0, "key fits"))
        return false;
    if (!assert_true(ctx, inv.add(bank, 4, 1) == 0, "phrasebook fits"))
        return false;
    if (!assert_true(ctx, inv.remove(1, 4) == 4, "remove 4 apples"))
        return false;
    log.log(EventType::ITEM_REMOVED, 1, 4, 0);
    if (!assert_true(ctx, inv.count_of(1) == 11, "11 apples remain"))
        return false;
    const uint32_t w = inv.weight(bank);
    if (!assert_true(ctx, w == 11u * 150u + 5u + 50u + 300u,
                     "weight matches contents"))
        return false;
    // Unknown id is rejected, never stored.
    if (!assert_true(ctx, inv.add(bank, 999, 1) == 1, "unknown id rejected"))
        return false;
    char buf[64]{};
    snap_u32(buf, sizeof(buf), "weight=", w);
    snap(ctx, state, cap, buf);
    return true;
}

// ---- Scenario 7: save/load (populated state round-trip) ----
static bool sc_save_load(AssertCtx& ctx, EventLog& log, char* state,
                         size_t cap, uint32_t) {
    const QuestBank qbank = content_quests();
    const ItemBank ibank = content_items();
    const VocabularyBank vbank = content_vocabulary();
    GameClock clock{};
    clock.day = 3;
    clock.minute = 545;
    PlayerState<8> player{};
    player.init(0x0103);
    Inventory<8> inv{};
    inv.init();
    player.bind_inventory(&inv);
    player.add_xp(2500);
    player.set_flag(5);
    player.add_counter(1, 7);
    if (inv.add(ibank, 1, 12) != 0) return false;
    if (inv.add(ibank, 2, 1) != 0) return false;
    QuestLog<8> qlog{};
    qlog.init();
    if (quest_start(qlog, qbank, 1) != QuestEvent::STARTED) return false;
    LanguageProfile<16> lang{};
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    lang.init(pair);
    if (!lang.record(vbank, 103, LanguageEvent::SEEN)) return false;
    if (!lang.record(vbank, 103, LanguageEvent::CORRECT)) return false;
    if (!lang.record(vbank, 203, LanguageEvent::USED)) return false;

    static char staging[2048]{};
    SaveInput<8, 8, 16> in{&clock, &player, &qlog, &lang};
    SaveResult why = SaveResult::MALFORMED;
    const size_t n = save_write(staging, sizeof(staging), in, &why);
    if (!assert_true(ctx, n > 0 && why == SaveResult::OK, "save serializes"))
        return false;
    log.log(EventType::SAVE, uint16_t(n & 0xFFFFu), 0, 0);
    if (!assert_true(ctx, save_validate(staging, n) == SaveResult::OK,
                     "staging validates"))
        return false;
    // Malformed staging never validates (atomic-commit gate).
    {
        char torn[2048]{};
        size_t i = 0;
        while (i < n && i < sizeof(torn)) {
            torn[i] = staging[i];
            ++i;
        }
        if (!assert_true(ctx,
                         save_validate(torn, n > 0 ? n - 1 : 0) == SaveResult::TRUNCATED,
                         "torn write rejected"))
            return false;
    }
    GameClock c2{};
    PlayerState<8> p2{};
    Inventory<8> inv2{};
    inv2.init();
    p2.bind_inventory(&inv2);
    QuestLog<8> q2{};
    q2.init();
    LanguageProfile<16> l2{};
    if (!assert_true(ctx,
                     save_read(staging, n, c2, p2, q2, l2, qbank, vbank, ibank) ==
                         SaveResult::OK,
                     "load applies"))
        return false;
    log.log(EventType::LOAD, uint16_t(n & 0xFFFFu), 0, 0);
    if (!assert_true(ctx, c2.day == 3 && c2.minute == 545, "clock restored"))
        return false;
    if (!assert_true(ctx, p2.xp == 2500 && p2.has_flag(5), "player restored"))
        return false;
    if (!assert_true(ctx, inv2.count_of(1) == 12 && inv2.count_of(2) == 1,
                     "inventory restored"))
        return false;
    const QuestRuntime* qr = q2.find(1);
    if (!assert_true(ctx, qr && qr->state == uint8_t(QuestState::ACTIVE),
                     "quest restored ACTIVE"))
        return false;
    const VocabularyProgress* s = l2.progress_of(103);
    if (!assert_true(ctx, s && s->exposures == 2 && s->correct == 1,
                     "language restored"))
        return false;
    // Unknown content id on load is strictly rejected.
    {
        static char bad[2048]{};
        size_t i = 0;
        while (i < n && i < sizeof(bad)) {
            bad[i] = staging[i];
            ++i;
        }
        // Corrupt the first SLOT id digit (same width: "SLOT 1 " -> "SLOT 9 ").
        size_t k = 0;
        bool patched = false;
        while (k + 8 < n) {
            if (bad[k] == 'S' && bad[k + 1] == 'L' && bad[k + 2] == 'O' &&
                bad[k + 3] == 'T' && bad[k + 4] == ' ') {
                bad[k + 5] = '9';
                patched = true;
                break;
            }
            ++k;
        }
        if (!assert_true(ctx, patched, "fixture locates SLOT line")) return false;
        GameClock c3{};
        PlayerState<8> p3{};
        Inventory<8> inv3{};
        inv3.init();
        p3.bind_inventory(&inv3);
        QuestLog<8> q3{};
        q3.init();
        LanguageProfile<16> l3{};
        if (!assert_true(ctx,
                         save_read(bad, n, c3, p3, q3, l3, qbank, vbank, ibank) ==
                             SaveResult::UNKNOWN_ID,
                         "unknown item id rejected"))
            return false;
    }
    char buf[64]{};
    snap_u32(buf, sizeof(buf), "bytes=", uint32_t(n));
    snap(ctx, state, cap, buf);
    return true;
}

// ---- Scenario 8: full vertical slice (real Scenario + content) ----
static bool open_slice_solid(void*, int32_t, int32_t) { return false; }

static bool sc_vertical_slice(AssertCtx& ctx, EventLog& log, char* state,
                              size_t cap, uint32_t) {
    Scenario sc{};
    const DialogueBank dbank = content_dialogues();
    const QuestBank qbank = content_quests();
    const ItemBank ibank = content_items();
    const VocabularyBank vbank = content_vocabulary();
    if (!assert_true(ctx, sc.init(&dbank, &qbank, &ibank, &vbank),
                     "slice scenario boots"))
        return false;
    log.log(EventType::GAME_START, 0, 0, 0);
    uint32_t now = 1000;
    // Approach Anna and open dialogue 1 (default greeting).
    sc.setPlayer(tr_at(17, 18));
    if (!assert_true(ctx, sc.pressE(now += 100), "E opens Anna dialogue"))
        return false;
    log.log(EventType::DIALOGUE_START, 1, 0, 0);
    if (!assert_true(ctx, sc.annaDialogue() == 1, "default greeting first"))
        return false;
    log.log(EventType::VARIANT_SELECTED, 1, 0, 0);
    if (!assert_true(ctx, sc.pressAnswer(1), "answer Yes")) return false;
    log.log(EventType::DIALOGUE_CHOICE, 1, 0, 0);
    const QuestRuntime* qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->state == uint8_t(QuestState::ACTIVE),
                     "START_QUEST effect fired"))
        return false;
    log.log(EventType::QUEST_STATE_CHANGED, 1, uint8_t(QuestState::ACTIVE), 0);
    if (!assert_true(ctx, sc.pressAnswer(1), "answer directions")) return false;
    if (!assert_true(ctx, sc.pressE(now += 100), "dismiss farewell")) return false;
    // Greet again: TALK fan-out completes objective 0.
    if (!assert_true(ctx, sc.pressE(now += 600), "greet again")) return false;
    qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->objective_idx == 1, "TALK objective done"))
        return false;
    log.log(EventType::QUEST_OBJECTIVE_CHANGED, 1, 1, 0);
    if (!assert_true(ctx, sc.pressE(now += 100), "E held during dialogue"))
        return false;
    // Walk away: dialogue aborts, interaction closes.
    sc.setPlayer(tr_at(4, 4));
    sc.tick(16, now += 16, open_slice_solid, nullptr);
    if (!assert_true(ctx, !sc.dialogueOpen(), "walk-away aborts")) return false;
    // Travel to the station district: REACH auto-reports.
    sc.setPlayer(tr_at(18, 18));
    sc.tick(16, now += 16, open_slice_solid, nullptr);
    qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->objective_idx == 2, "REACH reported"))
        return false;
    // Clerk: wrong answer holds, right answer hands the ticket.
    sc.setPlayer(tr_at(19, 18, 0xC000));
    if (!assert_true(ctx, sc.pressE(now += 600), "E opens clerk dialogue"))
        return false;
    if (!assert_true(ctx, sc.pressAnswer(3), "off-topic answered")) return false;
    log.log(EventType::RESPONSE_EVALUATED, uint8_t(ResponseVerdict::INCORRECT), 0, 0);
    if (!assert_true(ctx, sc.dialogueOpen(), "incorrect holds dialogue"))
        return false;
    if (!assert_true(ctx, sc.pressAnswer(1), "correct answer")) return false;
    if (!assert_true(ctx, sc.pressE(now += 100), "dismiss clerk farewell"))
        return false;
    sc.tick(16, now += 16, open_slice_solid, nullptr); // pump COLLECT
    qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->objective_idx == 3, "COLLECT done"))
        return false;
    // Hand over: GIVE branch needs no dialogue.
    if (!assert_true(ctx, sc.pressE(now += 600), "E hands ticket over")) return false;
    qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->state == uint8_t(QuestState::CLAIMED),
                     "quest auto-claimed"))
        return false;
    log.log(EventType::QUEST_STATE_CHANGED, 1, uint8_t(QuestState::CLAIMED), 0);
    if (!assert_true(ctx, sc.player().xp == 100, "reward +100 XP")) return false;
    // Save, keep playing from the file, verify restoration.
    if (!assert_true(ctx, sc.saveGame("playtest-slice.save"), "F5 saves"))
        return false;
    log.log(EventType::SAVE, 0, 0, 0);
    if (!assert_true(ctx, sc.loadGame("playtest-slice.save"), "F9 loads"))
        return false;
    log.log(EventType::LOAD, 0, 0, 0);
    qr = sc.quests().find(1);
    if (!assert_true(ctx, qr && qr->state == uint8_t(QuestState::CLAIMED),
                     "claim survives reload"))
        return false;
    if (!assert_true(ctx, sc.player().xp == 100, "xp survives reload"))
        return false;
    std::remove("playtest-slice.save");
    snap(ctx, state, cap, "slice=claimed,xp=100,save-load-ok");
    return true;
}

bool register_all_scenarios() {
    Registry& r = registry();
    bool ok = true;
    ok = register_scenario("basic-movement", "Basic movement + collision",
                           sc_basic_movement) &&
         ok;
    ok = register_scenario("npc-interaction", "NPC proximity + facing + E",
                           sc_npc_interaction) &&
         ok;
    ok = register_scenario("dialogue-evaluation",
                           "CORRECT/PARTIAL/INCORRECT verdicts",
                           sc_dialogue_evaluation) &&
         ok;
    ok = register_scenario("adaptive-dialogue",
                           "mastery-gated variant selection",
                           sc_adaptive_dialogue) &&
         ok;
    ok = register_scenario("quest-flow", "Station quest start to claim",
                           sc_quest_flow) &&
         ok;
    ok = register_scenario("inventory", "Starter items add/remove/count",
                           sc_inventory) &&
         ok;
    ok = register_scenario("save-load", "Populated round-trip + rejection",
                           sc_save_load) &&
         ok;
    ok = register_scenario("vertical-slice", "Full Anna-to-claim playthrough",
                           sc_vertical_slice) &&
         ok;
    return ok;
}

} // namespace playtest
} // namespace l3d
