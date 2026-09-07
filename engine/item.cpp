// Roadmap Phase 9 translation unit: registry lookup, validators,
// deterministic self-check. Unreferenced, item_selfcheck() is dropped by
// --gc-sections (firmware) or never pulled from the static lib.
#include "item.h"

namespace l3d {

const ItemDef* item_find(const ItemBank& bank, uint16_t id) {
    if (!bank.defs || id == ITEM_NONE) return nullptr;
    for (size_t i = 0; i < bank.count; ++i) {
        if (bank.defs[i].id == id) return &bank.defs[i];
    }
    return nullptr;
}

bool item_validate(const ItemDef& def) {
    if (def.id == 0 || def.id == ITEM_NONE) return false;
    if (def.category == uint8_t(ItemCategory::UNKNOWN) ||
        def.category >= uint8_t(ItemCategory::COUNT))
        return false;
    if (def.max_stack < 1) return false;
    if (!def.stackable && def.max_stack != 1) return false;
    if (!def.name || !def.name[0]) return false;
    size_t len = 0;
    while (def.name[len] && len <= ITEM_MAX_NAME) ++len;
    if (len == 0 || len > ITEM_MAX_NAME) return false;
    if (def.vocab_count > ITEM_MAX_VOCAB) return false;
    return true;
}

bool item_validate_bank(const ItemBank& bank, uint16_t* bad_id) {
    auto fail = [&](uint16_t id) {
        if (bad_id) *bad_id = id;
        return false;
    };
    if (!bank.defs && bank.count != 0) return fail(ITEM_NONE);
    for (size_t i = 0; i < bank.count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (bank.defs[j].id == bank.defs[i].id)
                return fail(bank.defs[i].id);
        }
        if (!item_validate(bank.defs[i])) return fail(bank.defs[i].id);
    }
    return true;
}

bool item_selfcheck() {
    static const char apple[] = "Apple";
    static const char key[] = "Key";
    static const ItemDef defs[] = {
        {1, uint8_t(ItemCategory::FOOD), true, 10, 150, apple, {201, 202}, 2},
        {2, uint8_t(ItemCategory::KEY), false, 1, 50, key, {}, 0},
    };
    static const ItemBank bank{defs, 2};
    if (!item_validate_bank(bank, nullptr)) return false;
    if (item_find(bank, 3) != nullptr) return false;

    Inventory<4> inv{};
    inv.init();
    if (!inv.empty()) return false;
    if (inv.add(bank, 1, 25) != 0) return false; // 10+10+5 in 3 slots
    if (inv.slots_used() != 3 || inv.count_of(1) != 25) return false;
    if (!inv.has(1, 25) || inv.has(1, 26)) return false;
    if (inv.weight(bank) != 25u * 150u) return false;
    if (inv.add(bank, 2, 1) != 0) return false; // 4th slot
    if (!inv.full()) return false;
    if (inv.add(bank, 2, 1) != 1) return false; // no room: full leftover
    if (inv.remove(1, 12) != 12) return false;  // 13 left
    if (inv.count_of(1) != 13) return false;
    if (inv.remove(9, 5) != 0) return false; // unknown id
    if (inv.add(bank, 9, 5) != 5) return false;
    inv.clear();
    if (!inv.empty() || inv.count_of(1) != 0) return false;
    return true;
}

} // namespace l3d
