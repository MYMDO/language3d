#pragma once
// Roadmap Phase 9: item/inventory foundation (no use/equip/consume/trade).
//
// Definitions are DATA (content/items/*.item compiled to static tables),
// instances are fixed stacks, inventories are fixed slot arrays. Items
// carry semantic hooks (category + opaque vocab tags referencing the shared
// content vocabulary namespace) but duplicate no language data: the future
// vocabulary subsystem resolves `Item -> vocab_id -> entry`.
//
// Dependency discipline: this unit includes nothing but core headers. It
// must never include dialogue/quest/npc/schedule units — subsystems stay
// composable and meet only through shared ids under future Game/Simulation
// orchestration (see docs/architecture.md invariant: no cycles).
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include <cstddef>
#include <cstdint>

namespace l3d {

constexpr uint16_t ITEM_NONE = 0xFFFFu;
constexpr size_t ITEM_MAX_VOCAB = 4;
constexpr size_t ITEM_MAX_NAME = 64;

enum class ItemCategory : uint8_t {
    UNKNOWN = 0,
    FOOD,
    TOOL,
    DOCUMENT,
    KEY,
    TICKET,
    BOOK,
    CLOTHING,
    CURRENCY,
    ARTIFACT,
    QUEST,
    COUNT
};

struct ItemDef {
    uint16_t id{0};
    uint8_t category{uint8_t(ItemCategory::UNKNOWN)};
    bool stackable{false};
    uint16_t max_stack{1};
    uint16_t weight{0}; // abstract units per piece (carried, not enforced)
    const char* name{nullptr};
    uint16_t vocab[ITEM_MAX_VOCAB]{};
    uint8_t vocab_count{0};
};

struct ItemBank {
    const ItemDef* defs{nullptr};
    size_t count{0};
};

// One inventory slot: a stack of a single item id (count==0 := empty).
struct ItemStack {
    uint16_t id{ITEM_NONE};
    uint16_t count{0};
};

const ItemDef* item_find(const ItemBank& bank, uint16_t id);
// Structural validation: nonzero id, known category, 1<=max_stack,
// non-stackable implies max_stack==1, non-empty short name, vocab in caps.
bool item_validate(const ItemDef& def);
bool item_validate_bank(const ItemBank& bank, uint16_t* bad_id);

template <size_t N>
struct Inventory {
    static_assert(N >= 1 && N <= 256, "inventory capacity out of range");
    ItemStack slots[N]{};
    size_t used{0}; // occupied slots (stacks), not pieces

    void init() {
        for (size_t i = 0; i < N; ++i) slots[i] = ItemStack{};
        used = 0;
    }

    bool empty() const { return used == 0; }
    bool full() const { return used >= N; }
    size_t slots_used() const { return used; }

    uint16_t count_of(uint16_t id) const {
        uint32_t total = 0;
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].id == id) total += slots[i].count;
        }
        return total > 0xFFFFu ? 0xFFFFu : uint16_t(total);
    }
    bool has(uint16_t id, uint16_t count = 1) const {
        return count_of(id) >= count;
    }

    // Add pieces; returns the leftover that did NOT fit (0 = all fit).
    // Unknown ids are rejected entirely (leftover == count).
    uint16_t add(const ItemBank& bank, uint16_t id, uint16_t count) {
        const ItemDef* def = item_find(bank, id);
        if (!def || count == 0) return count;
        uint16_t rest = count;
        // Top up existing stacks first (deterministic: slot order).
        if (def->stackable) {
            for (size_t i = 0; i < N && rest > 0; ++i) {
                if (slots[i].id != id || slots[i].count >= def->max_stack)
                    continue;
                const uint16_t room =
                    uint16_t(def->max_stack - slots[i].count);
                const uint16_t take = rest < room ? rest : room;
                slots[i].count = uint16_t(slots[i].count + take);
                rest = uint16_t(rest - take);
            }
        }
        // Open new stacks in the first free slots (slot order).
        const uint16_t per = def->stackable ? def->max_stack : 1;
        for (size_t i = 0; i < N && rest > 0; ++i) {
            if (slots[i].count != 0) continue;
            const uint16_t take = rest < per ? rest : per;
            slots[i].id = id;
            slots[i].count = take;
            rest = uint16_t(rest - take);
            ++used;
        }
        return rest;
    }

    // Remove pieces; returns how many were actually removed.
    uint16_t remove(uint16_t id, uint16_t count) {
        uint16_t gone = 0;
        for (size_t i = 0; i < N && gone < count; ++i) {
            if (slots[i].id != id) continue;
            const uint16_t want = uint16_t(count - gone);
            const uint16_t take =
                slots[i].count < want ? slots[i].count : want;
            slots[i].count = uint16_t(slots[i].count - take);
            gone = uint16_t(gone + take);
            if (slots[i].count == 0) {
                slots[i].id = ITEM_NONE;
                --used;
            }
        }
        return gone;
    }

    void clear() {
        for (size_t i = 0; i < N; ++i) slots[i] = ItemStack{};
        used = 0;
    }

    // Dry-run for transactional callers (quest rewards): would the whole
    // amount fit without mutating anything? Implemented on a local copy;
    // Inventory is trivially copyable and small (<= ~520 B).
    bool can_fit(const ItemBank& bank, uint16_t id, uint16_t count) const {
        Inventory probe = *this;
        return probe.add(bank, id, count) == 0;
    }

    // Total carried weight in abstract units (saturates at 32 bits).
    uint32_t weight(const ItemBank& bank) const {
        uint32_t total = 0;
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].count == 0) continue;
            const ItemDef* def = item_find(bank, slots[i].id);
            if (!def) continue;
            total += uint32_t(def->weight) * uint32_t(slots[i].count);
        }
        return total;
    }
};

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool item_selfcheck();

} // namespace l3d
