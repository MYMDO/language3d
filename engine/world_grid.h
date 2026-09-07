#pragma once
#include "assets.h"
#include "core.h"
#include <cstddef>

namespace l3d {

// Lightweight spatial view inspired by chunked world storage, but specialized
// for our 2.5D grid. The packed L3DP data remains immutable and zero-copy.
struct WorldGridView {
    static constexpr u8 CHUNK_SHIFT = 4; // 16x16 cells
    static constexpr u8 CHUNK_SIZE = 1u << CHUNK_SHIFT;

    const AssetPackView* pack{nullptr};
    const u8* map4_data{nullptr};
    u16 width{0};
    u16 height{0};
    u16 chunks_x{0};
    u16 chunks_y{0};

    bool init(const AssetPackView& assets) {
        if (!assets.valid() || !assets.map4()) return false;
        pack = &assets;
        map4_data = assets.map4();
        width = assets.map_width();
        height = assets.map_height();
        chunks_x = static_cast<u16>((width + CHUNK_SIZE - 1u) >> CHUNK_SHIFT);
        chunks_y = static_cast<u16>((height + CHUNK_SIZE - 1u) >> CHUNK_SHIFT);
        return width != 0 && height != 0;
    }

    bool valid() const { return pack && map4_data && width != 0 && height != 0; }

    inline u8 cell(i32 x, i32 y) const {
        if (x < 0 || y < 0 || x >= i32(width) || y >= i32(height)) return 1;
        const size_t idx = static_cast<size_t>(y) * width + static_cast<size_t>(x);
        const u8 byte = map4_data[idx >> 1];
        return (idx & 1u) ? static_cast<u8>(byte >> 4) : static_cast<u8>(byte & 0x0Fu);
    }

    inline u16 chunk_index(i32 x, i32 y) const {
        const u16 cx = static_cast<u16>(x >> CHUNK_SHIFT);
        const u16 cy = static_cast<u16>(y >> CHUNK_SHIFT);
        return static_cast<u16>(cy * chunks_x + cx);
    }

    inline bool in_bounds(i32 x, i32 y) const {
        return x >= 0 && y >= 0 && x < i32(width) && y < i32(height);
    }
};

} // namespace l3d
