#pragma once
#include "core.h"
#include <cstddef>

namespace l3d {

// Compact runtime entity representation intended for MCU-sized scenes.
struct SpriteEntity {
    Vec2 pos{};
    u8 texture{1};
    u8 flags{0};
    u8 width_scale{32};
    u8 height_scale{64};
};

static_assert(sizeof(SpriteEntity) == 12, "SpriteEntity must stay compact");

struct SpriteBatch {
    SpriteEntity* items{nullptr};
    size_t count{0};
    size_t capacity{0};

    bool push(const SpriteEntity& e) {
        if (!items || count >= capacity) return false;
        items[count++] = e;
        return true;
    }
    void clear() { count = 0; }
};

// No heap allocation: fixed-size pool for the default MCU scene.
template <size_t N>
class EntityPool {
public:
    bool add(const SpriteEntity& e) {
        if (count_ >= N) return false;
        items_[count_++] = e;
        return true;
    }
    void clear() { count_ = 0; }
    size_t size() const { return count_; }
    const SpriteEntity* data() const { return items_; }
    SpriteEntity* data() { return items_; }
private:
    SpriteEntity items_[N]{};
    size_t count_{0};
};

} // namespace l3d
