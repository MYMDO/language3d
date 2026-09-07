// Roadmap Phase 10 tests: player binding, progression, flags/counters,
// reset, snapshot/restore, inventory association, determinism.
#include "../engine/player.h"
#include "../engine/item.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

static const ItemBank test_bank() {
    static const char apple[] = "Apple";
    static const char ticket[] = "Ticket";
    static const ItemDef defs[] = {
        {1, uint8_t(ItemCategory::FOOD), true, 10, 150, apple, {}, 0},
        {2, uint8_t(ItemCategory::TICKET), false, 1, 5, ticket, {}, 0},
    };
    static const ItemBank bank{defs, 2};
    return bank;
}

int main() {
    const ItemBank bank = test_bank();

    // --- initialization + defaults ---
    PlayerState<8> p{};
    L3D_REQUIRE(!p.bound() && p.inventory == nullptr);
    L3D_REQUIRE(p.xp == 0 && p.level == 1 && p.reputation == 0);
    L3D_REQUIRE(p.flags == 0 && p.counter(0) == 0);

    // --- entity association (reference, never a copy) ---
    EntityPool<8> entities{};
    entities.init();
    const uint16_t e = entities.spawn(EntityKind::PLAYER);
    p.init(e);
    L3D_REQUIRE(p.bound() && p.entity == e);
    // Stale entity: the record keeps the id; liveness is the owner's job
    // (same discipline as every other pool in the codebase).
    L3D_REQUIRE(entities.destroy(e));
    L3D_REQUIRE(p.bound() && !entities.alive(p.entity));

    // --- inventory association is shared, not copied ---
    Inventory<8> inv{};
    inv.init();
    p.bind_inventory(&inv);
    L3D_REQUIRE(inv.add(bank, 1, 12) == 0);
    L3D_REQUIRE(p.inventory->count_of(1) == 12);
    L3D_REQUIRE(p.inventory->remove(1, 2) == 2);
    L3D_REQUIRE(inv.count_of(1) == 10); // same storage

    // --- progression: xp curve, reputation ---
    p.add_xp(999);
    L3D_REQUIRE(p.xp == 999 && p.level == 1);
    p.add_xp(1);
    L3D_REQUIRE(p.xp == 1000 && p.level == 2);
    p.add_xp(1000000);
    L3D_REQUIRE(p.level == 99); // capped placeholder curve
    p.reputation = 5;
    L3D_REQUIRE(p.reputation == 5);

    // --- flags: set/get/clear + bounds ---
    for (size_t b = 0; b < 32; ++b) L3D_REQUIRE(p.set_flag(b));
    L3D_REQUIRE(p.flags == 0xFFFFFFFFu);
    for (size_t b = 0; b < 32; ++b) L3D_REQUIRE(p.has_flag(b));
    L3D_REQUIRE(p.clear_flag(0) && !p.has_flag(0) && p.has_flag(31));
    L3D_REQUIRE(!p.set_flag(32) && !p.clear_flag(99) && !p.has_flag(32));

    // --- counters + bounds ---
    L3D_REQUIRE(p.add_counter(7, 60000));
    L3D_REQUIRE(p.add_counter(7, 60000) && p.counter(7) == 65535); // saturate
    L3D_REQUIRE(!p.add_counter(8, 1) && p.counter(8) == 0);

    // --- snapshot / restore round-trip ---
    PlayerPersistent snap = player_snapshot(p);
    L3D_REQUIRE(snap.version == PLAYER_SAVE_VERSION);
    L3D_REQUIRE(snap.xp == p.xp && snap.flags == p.flags);
    L3D_REQUIRE(snap.counters[7] == 65535);
    p.reset_progression();
    L3D_REQUIRE(p.xp == 0 && p.level == 1 && p.flags == 0);
    L3D_REQUIRE(p.counter(7) == 0 && p.reputation == 0);
    // Reset keeps bindings (entity ref + inventory association).
    L3D_REQUIRE(p.bound() && p.inventory == &inv);
    L3D_REQUIRE(player_restore(p, snap));
    L3D_REQUIRE(p.xp == snap.xp && p.level == snap.level);
    L3D_REQUIRE(p.flags == 0xFFFFFFFFu - 1u && p.counter(7) == 65535);
    snap.version = 0;
    L3D_REQUIRE(!player_restore(p, snap)); // version gate

    // --- determinism: same op sequence, identical snapshot ---
    PlayerState<8> a{}, b{};
    a.init(0x0101);
    b.init(0x0101);
    for (int run = 0; run < 2; ++run) {
        PlayerState<8>& v = (run == 0) ? a : b;
        v.add_xp(1500);
        v.set_flag(2);
        v.add_counter(1, 9);
    }
    const PlayerPersistent sa = player_snapshot(a);
    const PlayerPersistent sb = player_snapshot(b);
    L3D_REQUIRE(sa.xp == sb.xp && sa.flags == sb.flags && sa.counters[1] == 9);

    // --- unbind ---
    p.unbind();
    L3D_REQUIRE(!p.bound() && p.inventory == nullptr);

    L3D_REQUIRE(player_selfcheck());

    std::printf("player OK\n");
    return 0;
}
