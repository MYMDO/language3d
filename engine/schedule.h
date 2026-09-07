#pragma once
// Roadmap Phase 5: NPC schedules (data-oriented, no AI yet).
//
// A schedule binds an NPC id to an ordered list of time intervals, each
// pointing at a LOCATION ID — never at raw coordinates. Location ids live
// in the same opaque content namespace as Region3::contentTag: the world /
// content layer decides what HOME/SHOP/CAFE/PARK mean and where they are;
// the schedule only answers "where should this NPC be now, and doing what".
// The same mechanism later serves quest objectives, spawn points, shops,
// zones, dialogue context and language-topic context.
//
// Time is a deterministic game clock (day + minute), advanced with integer
// millisecond deltas — saveable plain data, reproducible tick-for-tick.
//
// Rules: no heap, no exceptions, no RTTI, no virtual dispatch, no STL.
#include "npc.h"

#include <cstddef>
#include <cstdint>

namespace l3d {

// Minutes per day and schedule capacity live here so content authors and
// the engine share one definition.
constexpr uint16_t MINUTES_PER_DAY = 1440;
constexpr size_t SCHEDULE_ENTRIES = 8;

// Behavior tag attached to an interval. Generic states only; NPC-role
// behavior (serve/sell/patrol…) arrives with the AI/dialogue phases.
enum class SchedBehavior : uint8_t {
    NONE = 0,
    STAY,
    WORK,
    REST,
    SOCIAL,
    COUNT
};

// Deterministic day/minute clock. ms_per_minute sets the time scale
// (default: 1 game-minute per real second); 0 disables advancement.
struct GameClock {
    uint32_t day{0};
    uint16_t minute{480}; // 08:00 default
    uint32_t ms_acc{0};
    uint32_t ms_per_minute{1000};

    void advance(uint32_t dt_ms) {
        if (ms_per_minute == 0) return;
        ms_acc += dt_ms;
        while (ms_acc >= ms_per_minute) {
            ms_acc -= ms_per_minute;
            ++minute;
            if (minute >= MINUTES_PER_DAY) {
                minute -= MINUTES_PER_DAY;
                ++day;
            }
        }
    }
};

struct ScheduleEntry {
    uint16_t start_min{0};   // inclusive
    uint16_t end_min{0};     // exclusive; end <= start means overnight wrap
    uint16_t locationTag{0}; // opaque content id (Region3 namespace)
    uint8_t behavior{uint8_t(SchedBehavior::STAY)};
};

template <size_t N, size_t E = SCHEDULE_ENTRIES>
struct SchedulePool {
    static_assert(N >= 1 && N <= 256, "schedule pool capacity out of range");
    static_assert(E >= 1 && E <= 64, "schedule entry count out of range");

    struct Slot {
        uint16_t npc{L3D_NPC_INVALID}; // full generational NPC id; INVALID = free
        ScheduleEntry entries[E]{};
        size_t count{0};
    };

    Slot slots[N]{};
    size_t live{0};

    void init() {
        for (size_t i = 0; i < N; ++i) slots[i] = Slot{};
        live = 0;
    }

    // Create or replace the schedule for an NPC id. Rejects INVALID ids,
    // over-long tables, and (for new NPCs) a full pool.
    bool set(uint16_t npc, const ScheduleEntry* entries, size_t count) {
        if (npc == L3D_NPC_INVALID || !entries || count == 0 || count > E)
            return false;
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].npc == npc) {
                copy_entries(i, entries, count);
                return true;
            }
        }
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].npc == L3D_NPC_INVALID) {
                copy_entries(i, entries, count);
                slots[i].npc = npc;
                ++live;
                return true;
            }
        }
        return false;
    }

    bool clear(uint16_t npc) {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].npc == npc) {
                slots[i] = Slot{};
                --live;
                return true;
            }
        }
        return false;
    }

    // Current entry for an NPC id at a minute-of-day, or nullptr when the
    // NPC has no schedule / nothing covers the minute. First covering entry
    // in authoring order wins (deterministic priority).
    const ScheduleEntry* lookup(uint16_t npc, uint16_t minute) const {
        for (size_t i = 0; i < N; ++i) {
            if (slots[i].npc != npc) continue;
            for (size_t k = 0; k < slots[i].count; ++k) {
                const ScheduleEntry& e = slots[i].entries[k];
                if (covers(e, minute)) return &e;
            }
            return nullptr;
        }
        return nullptr;
    }

    size_t live_count() const { return live; }
    bool full() const { return live >= N; }
    bool empty() const { return live == 0; }

    static bool covers(const ScheduleEntry& e, uint16_t minute) {
        if (e.end_min > e.start_min)
            return minute >= e.start_min && minute < e.end_min;
        // Overnight wrap, e.g. 21:00 -> 08:00.
        return minute >= e.start_min || minute < e.end_min;
    }

  private:
    void copy_entries(size_t i, const ScheduleEntry* entries, size_t count) {
        for (size_t k = 0; k < count; ++k) slots[i].entries[k] = entries[k];
        slots[i].count = count;
    }
};

// Deterministic self-check (no I/O, no heap). See entity_selfcheck().
bool schedule_selfcheck();

} // namespace l3d
