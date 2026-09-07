// Roadmap Phase 5 translation unit.
//
// Compile-proves the schedule pool and game clock for every linked
// target's toolchain. schedule_selfcheck() is deterministic and I/O-free;
// unreferenced, it is dropped by --gc-sections (firmware) or never pulled
// from the static lib.
#include "schedule.h"

namespace l3d {

bool schedule_selfcheck() {
    GameClock clock{};
    if (clock.day != 0 || clock.minute != 480) return false;
    clock.advance(1500); // 1.5 s at 1000 ms/min
    if (clock.minute != 481 || clock.ms_acc != 500) return false;
    clock.advance(500);
    if (clock.minute != 482 || clock.ms_acc != 0) return false;

    // Day rollover: 23:59 + 2 min -> day 1, 00:01.
    clock.minute = 1439;
    clock.advance(2000);
    if (clock.day != 1 || clock.minute != 1) return false;

    SchedulePool<4> pool{};
    pool.init();
    if (!pool.empty()) return false;

    // Anna's day: home overnight, shop, cafe, shop, park, home.
    // Location tags are content ids (defined by the content layer).
    constexpr uint16_t HOME = 1, SHOP = 2, CAFE = 3, PARK = 4;
    const ScheduleEntry day[] = {
        {1260, 480, HOME, uint8_t(SchedBehavior::REST)},   // 21:00-08:00
        {480, 540, HOME, uint8_t(SchedBehavior::STAY)},    // 08:00-09:00
        {540, 780, SHOP, uint8_t(SchedBehavior::WORK)},    // 09:00-13:00
        {780, 840, CAFE, uint8_t(SchedBehavior::SOCIAL)},  // 13:00-14:00
        {840, 1080, SHOP, uint8_t(SchedBehavior::WORK)},   // 14:00-18:00
        {1080, 1260, PARK, uint8_t(SchedBehavior::STAY)},  // 18:00-21:00
    };
    const uint16_t anna = entity_make_id(1, 0);
    if (!pool.set(anna, day, 6)) return false;
    const ScheduleEntry* e = nullptr;
    e = pool.lookup(anna, 480); // boundary: start inclusive
    if (!e || e->locationTag != HOME) return false;
    e = pool.lookup(anna, 540); // shop opens
    if (!e || e->locationTag != SHOP) return false;
    e = pool.lookup(anna, 780); // boundary: end exclusive -> next entry
    if (!e || e->locationTag != CAFE) return false;
    e = pool.lookup(anna, 60); // 01:00 overnight
    if (!e || e->locationTag != HOME) return false;
    e = pool.lookup(anna, 1200); // 20:00 park
    if (!e || e->locationTag != PARK) return false;
    if (pool.lookup(0x1234, 600) != nullptr) return false; // stale NPC
    if (!pool.clear(anna)) return false;
    if (pool.lookup(anna, 600) != nullptr) return false;
    return pool.empty();
}

} // namespace l3d
