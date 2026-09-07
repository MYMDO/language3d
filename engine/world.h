#pragma once
#include "assets.h"
#include "core.h"
#include <cstddef>

namespace l3d {

// Runtime state for the small number of dynamic doors in an MCU-sized map.
// The packed map stays immutable; only these tiny records change at runtime.
struct DoorState {
    u8 x{0};
    u8 y{0};
    u8 material{15};
    u8 open{0};       // 0 = closed, 255 = fully open
    bool moving{false};
    bool target_open{false};
    u16 accumulator_ms{0};
};

template <size_t N>
class DoorSystem {
public:
    void init(const AssetPackView& assets) {
        count_ = 0;
        for (u16 y = 0; y < assets.map_height() && count_ < N; ++y) {
            for (u16 x = 0; x < assets.map_width() && count_ < N; ++x) {
                if (packed_map_cell4(assets, x, y) == DOOR_MATERIAL) {
                    storage_[count_++] = DoorState{static_cast<u8>(x), static_cast<u8>(y), DOOR_MATERIAL, 0, false, false, 0};
                }
            }
        }
    }

    void update(u32 dt_ms) {
        constexpr u16 SPEED = 102; // ~2.5 seconds from 0..255 at 102 units/s
        for (size_t i = 0; i < count_; ++i) {
            auto& d = storage_[i];
            if (!d.moving) continue;
            const u32 accum = u32(d.accumulator_ms) + u32(SPEED) * dt_ms;
            const u16 step = static_cast<u16>(accum / 1000u);
            d.accumulator_ms = static_cast<u16>(accum % 1000u);
            if (step == 0) continue;
            if (d.target_open) {
                const u16 next = static_cast<u16>(d.open) + step;
                d.open = static_cast<u8>(next >= 255u ? 255u : next);
                if (d.open == 255u) d.moving = false;
            } else {
                const i16 next = static_cast<i16>(d.open) - static_cast<i16>(step);
                d.open = static_cast<u8>(next <= 0 ? 0 : next);
                if (d.open == 0) d.moving = false;
            }
        }
    }

    bool toggle_near(const Vec2& p, u8 radius_cells = 1) {
        for (size_t i = 0; i < count_; ++i) {
            auto& d = storage_[i];
            const i32 dx = d.x - p.x.to_int();
            const i32 dy = d.y - p.y.to_int();
            if (dx < -i32(radius_cells) || dx > i32(radius_cells) ||
                dy < -i32(radius_cells) || dy > i32(radius_cells)) continue;
            d.target_open = !d.target_open;
            d.moving = true;
            return true;
        }
        return false;
    }

    bool blocks(i32 x, i32 y, const AssetPackView& assets) const {
        const u8 material = packed_map_cell4(assets, x, y);
        if (material != DOOR_MATERIAL) return material != 0;
        const DoorState* d = find(x, y);
        return d ? d->open < 255 : true;
    }

    bool is_fully_open(i32 x, i32 y) const {
        const DoorState* d = find(x, y);
        return d && d->open == 255;
    }

    size_t size() const { return count_; }
    const DoorState* data() const { return storage_; }
    static constexpr u8 DOOR_MATERIAL = 15;

private:
    const DoorState* find(i32 x, i32 y) const {
        for (size_t i = 0; i < count_; ++i)
            if (storage_[i].x == x && storage_[i].y == y) return &storage_[i];
        return nullptr;
    }
    DoorState storage_[N]{};
    size_t count_{0};
};

} // namespace l3d
