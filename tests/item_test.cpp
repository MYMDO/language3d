// Roadmap Phase 9 tests: generated item bank, inventory add/remove/has/
// count/clear/weight, stacking rules, validators, selfcheck.
#include "../engine/item.h"
#include "test_common.h"
#include <cstdio>
#include <cstring>

using namespace l3d;

namespace l3d {
const ItemBank content_items(); // generated_items.cpp
}

int main() {
    // --- generated bank validates and matches authored content ---
    const ItemBank bank = content_items();
    L3D_REQUIRE(bank.count == 4);
    uint16_t bad = 0;
    L3D_REQUIRE(item_validate_bank(bank, &bad));
    const ItemDef* apple = item_find(bank, 1);
    L3D_REQUIRE(apple && apple->category == uint8_t(ItemCategory::FOOD));
    L3D_REQUIRE(apple->stackable && apple->max_stack == 10);
    L3D_REQUIRE(std::strcmp(apple->name, "Apple") == 0);
    L3D_REQUIRE(apple->vocab_count == 2 && apple->vocab[0] == 201);
    const ItemDef* ticket = item_find(bank, 2);
    L3D_REQUIRE(ticket && !ticket->stackable && ticket->max_stack == 1);
    L3D_REQUIRE(item_find(bank, 999) == nullptr);

    // --- stacking: top-up in slot order, then new stacks ---
    Inventory<4> inv{};
    inv.init();
    L3D_REQUIRE(inv.empty() && !inv.full());
    L3D_REQUIRE(inv.add(bank, 1, 7) == 0);
    L3D_REQUIRE(inv.add(bank, 1, 8) == 0); // 10 + 5 across 2 slots
    L3D_REQUIRE(inv.slots_used() == 2 && inv.count_of(1) == 15);
    L3D_REQUIRE(inv.has(1, 15) && !inv.has(1, 16) && !inv.has(2));

    // --- non-stackables occupy one slot each ---
    L3D_REQUIRE(inv.add(bank, 2, 1) == 0);
    L3D_REQUIRE(inv.add(bank, 3, 1) == 0);
    L3D_REQUIRE(inv.full());
    L3D_REQUIRE(inv.add(bank, 4, 1) == 1); // no room: full leftover
    L3D_REQUIRE(inv.count_of(4) == 0);

    // --- weight accounting ---
    const uint32_t expect_w = 15u * 150u + 1u * 5u + 1u * 50u;
    L3D_REQUIRE(inv.weight(bank) == expect_w);

    // --- removal compacts slots, unknown ids are safe ---
    L3D_REQUIRE(inv.remove(1, 12) == 12); // 10+2 drained, 3 left
    L3D_REQUIRE(inv.count_of(1) == 3 && inv.slots_used() == 3);
    L3D_REQUIRE(inv.remove(1, 99) == 3); // more than held
    L3D_REQUIRE(!inv.has(1) && inv.slots_used() == 2);
    L3D_REQUIRE(inv.remove(777, 1) == 0);
    L3D_REQUIRE(inv.add(bank, 777, 2) == 2); // unknown def rejected

    // --- clear ---
    inv.clear();
    L3D_REQUIRE(inv.empty() && inv.count_of(2) == 0 && inv.weight(bank) == 0);

    // --- runtime validator negatives ---
    {
        static const char n[] = "X";
        static const ItemDef dup_a{5, 1, true, 5, 1, n, {}, 0};
        static const ItemDef dup_b{5, 1, true, 5, 1, n, {}, 0};
        static const ItemDef both[] = {dup_a, dup_b};
        const ItemBank dup_bank{both, 2};
        uint16_t bad_id = 0;
        L3D_REQUIRE(!item_validate_bank(dup_bank, &bad_id) && bad_id == 5);
        static const ItemDef bad_stack{6, 2, false, 5, 1, n, {}, 0};
        L3D_REQUIRE(!item_validate(bad_stack)); // non-stackable, max != 1
        static const ItemDef bad_cat{7, 99, true, 5, 1, n, {}, 0};
        L3D_REQUIRE(!item_validate(bad_cat));
        static const ItemDef no_name{8, 1, true, 5, 1, nullptr, {}, 0};
        L3D_REQUIRE(!item_validate(no_name));
    }

    // --- determinism: same op sequence, same state ---
    Inventory<4> a{}, b{};
    a.init();
    b.init();
    for (int run = 0; run < 2; ++run) {
        Inventory<4>& v = (run == 0) ? a : b;
        v.add(bank, 1, 23);
        v.add(bank, 4, 1);
        v.remove(1, 4);
    }
    L3D_REQUIRE(a.slots_used() == b.slots_used());
    L3D_REQUIRE(a.count_of(1) == b.count_of(1) && a.count_of(4) == 1);
    L3D_REQUIRE(a.weight(bank) == b.weight(bank));

    L3D_REQUIRE(item_selfcheck());

    std::printf("item OK\n");
    return 0;
}
