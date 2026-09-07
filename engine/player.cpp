// Roadmap Phase 10 translation unit: deterministic self-check.
// Unreferenced, player_selfcheck() is dropped by --gc-sections.
#include "player.h"
#include "item.h"

namespace l3d {

bool player_selfcheck() {
    static const char apple[] = "Apple";
    static const ItemDef defs[] = {
        {1, uint8_t(ItemCategory::FOOD), true, 10, 150, apple, {}, 0},
    };
    static const ItemBank bank{defs, 1};

    PlayerState<4> p{};
    if (p.bound()) return false; // starts unbound
    p.init(0x0105);
    if (!p.bound()) return false;

    Inventory<4> inv{};
    inv.init();
    p.bind_inventory(&inv);
    if (inv.add(bank, 1, 7) != 0) return false;
    if (!p.inventory || p.inventory->count_of(1) != 7) return false; // shared

    p.add_xp(2500);
    if (p.xp != 2500 || p.level != 3) return false; // 1 + 2500/1000
    if (!p.set_flag(3) || !p.has_flag(3)) return false;
    if (!p.clear_flag(3) || p.has_flag(3)) return false;
    if (p.set_flag(32) || p.has_flag(40)) return false; // out of range
    if (!p.add_counter(0, 5) || p.counter(0) != 5) return false;
    if (p.add_counter(8, 1) || p.counter(8) != 0) return false;

    PlayerPersistent snap = player_snapshot(p);
    if (snap.version != PLAYER_SAVE_VERSION || snap.entity != 0x0105) return false;
    if (snap.xp != 2500 || snap.counters[0] != 5) return false;

    p.reset_progression();
    if (p.xp != 0 || p.level != 1 || p.counter(0) != 0) return false;
    if (!player_restore(p, snap)) return false; // bindings preserved
    if (p.entity != 0x0105 || p.inventory != &inv) return false;
    if (p.xp != 2500 || p.level != 3 || p.counter(0) != 5) return false;

    PlayerPersistent bad = snap;
    bad.version = 99;
    if (player_restore(p, bad)) return false; // version gate

    p.unbind();
    if (p.bound()) return false;
    return true;
}

} // namespace l3d
