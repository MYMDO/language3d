// Roadmap Phase 5 tests: deterministic game clock, location-ID schedule
// lookup (incl. overnight wrap and boundaries), capacity, stale rejection.
#include "../engine/schedule.h"
#include "test_common.h"
#include <cstdio>

using namespace l3d;

// Content-layer location ids (defined by content, not the engine).
constexpr uint16_t HOME = 1, SHOP = 2, CAFE = 3, PARK = 4;

static void annas_day(ScheduleEntry* out) {
    out[0] = {1260, 480, HOME, uint8_t(SchedBehavior::REST)};
    out[1] = {480, 540, HOME, uint8_t(SchedBehavior::STAY)};
    out[2] = {540, 780, SHOP, uint8_t(SchedBehavior::WORK)};
    out[3] = {780, 840, CAFE, uint8_t(SchedBehavior::SOCIAL)};
    out[4] = {840, 1080, SHOP, uint8_t(SchedBehavior::WORK)};
    out[5] = {1080, 1260, PARK, uint8_t(SchedBehavior::STAY)};
}

int main() {
    // --- clock: exact accumulation, determinism, rollover ---
    {
        GameClock a{}, b{};
        const uint32_t steps[] = {16, 17, 250, 1000, 33, 7};
        for (size_t i = 0; i < 6; ++i) {
            a.advance(steps[i]);
            b.advance(steps[i]);
        }
        L3D_REQUIRE(a.day == b.day && a.minute == b.minute && a.ms_acc == b.ms_acc);
        L3D_REQUIRE(a.minute == 481 && a.ms_acc == 323); // 1323 ms total
        GameClock r{};
        r.minute = 1439;
        r.advance(60000); // +60 min
        L3D_REQUIRE(r.day == 1 && r.minute == 59);
        GameClock frozen{};
        frozen.ms_per_minute = 0;
        frozen.advance(1000000);
        L3D_REQUIRE(frozen.minute == 480 && frozen.day == 0);
    }

    SchedulePool<4> pool{};
    pool.init();
    L3D_REQUIRE(pool.empty() && !pool.full());

    ScheduleEntry day[6]{};
    annas_day(day);
    const uint16_t anna = entity_make_id(1, 0);
    const uint16_t bob = entity_make_id(1, 1);

    // --- validation ---
    L3D_REQUIRE(!pool.set(L3D_NPC_INVALID, day, 6));
    L3D_REQUIRE(!pool.set(anna, nullptr, 6));
    L3D_REQUIRE(!pool.set(anna, day, 0));
    L3D_REQUIRE(!pool.set(anna, day, 65)); // over capacity
    L3D_REQUIRE(pool.set(anna, day, 6));

    // --- lookup across the whole day ---
    const struct {
        uint16_t minute;
        uint16_t place;
    } probes[] = {
        {0, HOME}, {479, HOME}, {480, HOME}, {539, HOME}, {540, SHOP},
        {779, SHOP}, {780, CAFE}, {839, CAFE}, {840, SHOP}, {1079, SHOP},
        {1080, PARK}, {1259, PARK}, {1260, HOME}, {1439, HOME},
    };
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
        const ScheduleEntry* e = pool.lookup(anna, probes[i].minute);
        L3D_REQUIRE(e && e->locationTag == probes[i].place);
    }
    // Behavior tags travel with entries.
    L3D_REQUIRE(pool.lookup(anna, 600)->behavior == uint8_t(SchedBehavior::WORK));
    L3D_REQUIRE(pool.lookup(anna, 800)->behavior == uint8_t(SchedBehavior::SOCIAL));

    // --- stale / missing ---
    L3D_REQUIRE(pool.lookup(bob, 600) == nullptr);
    L3D_REQUIRE(pool.lookup(0x1234, 600) == nullptr);
    L3D_REQUIRE(!pool.clear(bob));

    // --- replace + capacity + clear ---
    ScheduleEntry short_day[2] = {
        {0, 720, HOME, uint8_t(SchedBehavior::REST)},
        {720, 1440, PARK, uint8_t(SchedBehavior::STAY)},
    };
    L3D_REQUIRE(pool.set(anna, short_day, 2)); // replace in place
    L3D_REQUIRE(pool.lookup(anna, 600)->locationTag == HOME);
    L3D_REQUIRE(pool.lookup(anna, 800)->locationTag == PARK);
    L3D_REQUIRE(pool.live_count() == 1);
    ScheduleEntry one[1] = {{0, 1440, SHOP, uint8_t(SchedBehavior::WORK)}};
    L3D_REQUIRE(pool.set(bob, one, 1));
    L3D_REQUIRE(pool.set(entity_make_id(1, 2), one, 1));
    L3D_REQUIRE(pool.set(entity_make_id(1, 3), one, 1));
    L3D_REQUIRE(pool.full());
    L3D_REQUIRE(!pool.set(entity_make_id(1, 4), one, 1));
    L3D_REQUIRE(pool.clear(anna));
    L3D_REQUIRE(pool.lookup(anna, 600) == nullptr);
    L3D_REQUIRE(!pool.empty());

    L3D_REQUIRE(schedule_selfcheck());

    std::printf("schedule OK\n");
    return 0;
}
