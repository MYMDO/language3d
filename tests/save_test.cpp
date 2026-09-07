// Roadmap Phase 13 tests: roundtrips, sections, determinism, rejection
// paths, migration gate, atomic-failure behavior, selfcheck.
#include "../engine/save.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>

using namespace l3d;

namespace l3d {
const QuestBank content_quests();
const ItemBank content_items();
const VocabularyBank content_vocabulary();
}

// Populated world: clock + player + stocked inventory + mid quest +
// language progress. Mirrors a real gameplay moment.
struct World {
    GameClock clock{};
    PlayerState<8> player{};
    Inventory<8> inv{};
    QuestLog<8> log{};
    LanguageProfile<16> lang{};
};

static void populate(World& w) {
    w.clock.day = 3;
    w.clock.minute = 545;
    w.clock.ms_acc = 250;
    w.player.init(0x0103);
    w.inv.init();
    w.player.bind_inventory(&w.inv);
    w.player.add_xp(2500);
    w.player.set_flag(5);
    w.player.add_counter(1, 7);
    w.log.init();
    LanguagePair pair{uint8_t(DialogueLang::PL), uint8_t(DialogueLang::EN)};
    w.lang.init(pair);
}

static size_t write_save(char* buf, size_t cap, World& w, SaveResult* why = nullptr) {
    SaveInput<8, 8, 16> in{&w.clock, &w.player, &w.log, &w.lang};
    return save_write(buf, cap, in, why);
}

int main() {
    const QuestBank qbank = content_quests();
    const ItemBank ibank = content_items();
    const VocabularyBank vbank = content_vocabulary();
    static char staging[2048]{};

    // --- empty state roundtrip ---
    {
        World w{};
        populate(w); // binds everything, keeps values default
        w.player.reset_progression();
        SaveResult why = SaveResult::MALFORMED;
        const size_t n = write_save(staging, sizeof(staging), w, &why);
        L3D_REQUIRE(n > 0 && why == SaveResult::OK);
        L3D_REQUIRE(save_validate(staging, n) == SaveResult::OK);
        GameClock c2{};
        PlayerState<8> p2{};
        Inventory<8> inv2{};
        inv2.init();
        p2.bind_inventory(&inv2);
        QuestLog<8> q2{};
        q2.init();
        LanguageProfile<16> l2{};
        L3D_REQUIRE(save_read(staging, n, c2, p2, q2, l2, qbank, vbank, ibank) ==
                    SaveResult::OK);
        L3D_REQUIRE(c2.day == 3 && c2.minute == 545 && c2.ms_acc == 250);
        L3D_REQUIRE(p2.entity == 0x0103 && p2.xp == 0 && p2.level == 1);
        L3D_REQUIRE(!p2.has_flag(5) && p2.counter(1) == 0);
        L3D_REQUIRE(inv2.empty() && q2.find(1) == nullptr);
    }

    // --- populated state roundtrip (inventory + quest + language) ---
    World w{};
    populate(w);
    L3D_REQUIRE(w.inv.add(ibank, 1, 12) == 0); // apples 10+2
    L3D_REQUIRE(w.inv.add(ibank, 2, 1) == 0);  // ticket
    L3D_REQUIRE(quest_start(w.log, qbank, 1) == QuestEvent::STARTED);
    L3D_REQUIRE(w.lang.record(vbank, 103, LanguageEvent::SEEN));
    L3D_REQUIRE(w.lang.record(vbank, 103, LanguageEvent::CORRECT));
    L3D_REQUIRE(w.lang.record(vbank, 203, LanguageEvent::USED));
    SaveResult why = SaveResult::MALFORMED;
    const size_t n = write_save(staging, sizeof(staging), w, &why);
    L3D_REQUIRE(n > 0 && why == SaveResult::OK);
    // Serialized size of a real moment stays tiny (embedded-friendly).
    L3D_REQUIRE(n < 512);
    L3D_REQUIRE(save_validate(staging, n) == SaveResult::OK);
    // Determinism: same state, identical bytes.
    static char staging2[2048]{};
    L3D_REQUIRE(write_save(staging2, sizeof(staging2), w, nullptr) == n);
    L3D_REQUIRE(std::memcmp(staging, staging2, n) == 0);

    GameClock c2{};
    PlayerState<8> p2{};
    Inventory<8> inv2{};
    inv2.init();
    p2.bind_inventory(&inv2);
    QuestLog<8> q2{};
    q2.init();
    LanguageProfile<16> l2{};
    L3D_REQUIRE(save_read(staging, n, c2, p2, q2, l2, qbank, vbank, ibank) ==
                SaveResult::OK);
    L3D_REQUIRE(c2.day == 3 && c2.minute == 545);
    L3D_REQUIRE(p2.entity == 0x0103 && p2.xp == 2500 && p2.level == 3);
    L3D_REQUIRE(p2.has_flag(5) && p2.counter(1) == 7);
    L3D_REQUIRE(inv2.count_of(1) == 12 && inv2.count_of(2) == 1);
    L3D_REQUIRE(inv2.slots_used() == 3);
    const QuestRuntime* qr = q2.find(1);
    L3D_REQUIRE(qr && qr->state == uint8_t(QuestState::ACTIVE));
    const VocabularyProgress* s = l2.progress_of(103);
    L3D_REQUIRE(s && s->exposures == 2 && s->correct == 1);
    L3D_REQUIRE(mastery_of(*s) == Mastery::INTRODUCED);
    L3D_REQUIRE(l2.progress_of(203)->uses == 1);
    L3D_REQUIRE(l2.pair.source == uint8_t(DialogueLang::PL));

    // --- invalid version / migration gate ---
    {
        static char bad[2048]{};
        std::memcpy(bad, staging, n);
        // "L3DSAVE 1\n" -> "L3DSAVE 2\n": well-formed, needs migration.
        bad[8] = '2';
        L3D_REQUIRE(save_validate(bad, n) == SaveResult::NEEDS_MIGRATION);
        bad[8] = 'X';
        L3D_REQUIRE(save_validate(bad, n) == SaveResult::BAD_VERSION);
    }

    // --- malformed saves ---
    {
        const char no_end[] = "L3DSAVE 1\nCLOCK 0 0 0 1000\n";
        L3D_REQUIRE(save_validate(no_end, sizeof(no_end) - 1) == SaveResult::TRUNCATED);
        static char bad[2048]{};
        std::memcpy(bad, staging, n);
        bad[10] = 'X'; // corrupt CLOCK keyword
        L3D_REQUIRE(save_validate(bad, n) == SaveResult::MALFORMED);
        std::memcpy(bad, staging, n);
        // Out-of-order sections: swap the PLAYER and COUNTERS lines.
        const char* l1 = std::strchr(staging, '\n') + 1; // CLOCK line
        const char* l2 = std::strchr(l1, '\n') + 1;      // PLAYER line
        const char* l3 = std::strchr(l2, '\n') + 1;      // COUNTERS line
        const char* l4 = std::strchr(l3, '\n') + 1;      // next line
        const size_t len2 = size_t(l3 - l2);
        const size_t len3 = size_t(l4 - l3);
        char tmp[2048]{};
        const size_t pre = size_t(l2 - staging);
        std::memcpy(tmp, staging, pre);
        std::memcpy(tmp + pre, l3, len3);
        std::memcpy(tmp + pre + len3, l2, len2);
        std::memcpy(tmp + pre + len3 + len2, l4, n - pre - len3 - len2);
        L3D_REQUIRE(save_validate(tmp, n) == SaveResult::MALFORMED);
    }

    // --- truncated save (torn write) never validates ---
    for (size_t cut = 0; cut < n; cut += 37) {
        const SaveResult r = save_validate(staging, cut);
        L3D_REQUIRE(r == SaveResult::TRUNCATED || r == SaveResult::MALFORMED ||
                    r == SaveResult::BAD_VERSION);
        L3D_REQUIRE(r != SaveResult::OK);
    }
    L3D_REQUIRE(save_validate(staging, n - 1) == SaveResult::TRUNCATED);

    // --- invalid content ids rejected on load ---
    {
        // Quest id 7 does not exist in the bank: patch the stored "QUEST 1".
        static char bad[2048]{};
        std::memcpy(bad, staging, n);
        char* qpos = std::strstr(bad, "QUEST 1 1 0 0\n");
        L3D_REQUIRE(qpos != nullptr);
        qpos[6] = '7'; // same width: "QUEST 7 1 0 0"
        L3D_REQUIRE(save_validate(bad, n) == SaveResult::OK); // grammar fine
        GameClock c3{};
        PlayerState<8> p3{};
        Inventory<8> inv3{};
        inv3.init();
        p3.bind_inventory(&inv3);
        QuestLog<8> q3{};
        q3.init();
        LanguageProfile<16> l3{};
        L3D_REQUIRE(save_read(bad, n, c3, p3, q3, l3, qbank, vbank, ibank) ==
                    SaveResult::UNKNOWN_ID);
        // Unknown word id: patch WORD 103 -> 900 (same width).
        std::memcpy(bad, staging, n);
        char* wpos = std::strstr(bad, "WORD 103 ");
        L3D_REQUIRE(wpos != nullptr);
        std::memcpy(wpos + 5, "900", 3); // "WORD 900 ..."
        L3D_REQUIRE(save_read(bad, n, c3, p3, q3, l3, qbank, vbank, ibank) ==
                    SaveResult::UNKNOWN_ID);
        // Unknown item id: patch SLOT 1 -> SLOT 9 (same width).
        std::memcpy(bad, staging, n);
        char* spos = std::strstr(bad, "SLOT 1 ");
        L3D_REQUIRE(spos != nullptr);
        spos[5] = '9';
        L3D_REQUIRE(save_read(bad, n, c3, p3, q3, l3, qbank, vbank, ibank) ==
                    SaveResult::UNKNOWN_ID);
    }

    // --- stale entity reference persists opaquely; owner rebinds ---
    {
        GameClock c4{};
        PlayerState<8> p4{};
        Inventory<8> inv4{};
        inv4.init();
        p4.bind_inventory(&inv4);
        QuestLog<8> q4{};
        q4.init();
        LanguageProfile<16> l4{};
        L3D_REQUIRE(save_read(staging, n, c4, p4, q4, l4, qbank, vbank, ibank) ==
                    SaveResult::OK);
        L3D_REQUIRE(p4.entity == 0x0103); // kept as-is, liveness is owner's job
    }

    // --- staging overflow path ---
    {
        char tiny[16]{};
        SaveResult r = SaveResult::OK;
        L3D_REQUIRE(write_save(tiny, sizeof(tiny), w, &r) == 0);
        L3D_REQUIRE(r == SaveResult::NO_SPACE);
    }

    L3D_REQUIRE(save_selfcheck());

    std::printf("save OK\n");
    return 0;
}
