#pragma once
#include "core.h"
#include <cstddef>

namespace l3d {

// Compact runtime asset container. The engine never owns the backing storage.
struct AssetPackView {
    const u8* data{nullptr};
    size_t size{0};

    bool valid() const;
    const u8* map4() const;
    u16 map_width() const;
    u16 map_height() const;
    u8 texture_count() const;
    const u8* texture8(u8 index) const;
};

// On-device format: 16-byte header + nibble-packed map + 64x64 8-bit textures.
// L3DP v1: 16-byte header with u8 map dimensions.
// L3DP v2: 20-byte header with u16 map dimensions and u32 map/texture byte counts.
bool asset_pack_validate(const u8* data, size_t size);

u8 packed_map_cell4(const AssetPackView& pack, i32 x, i32 y);

// Hot-path accessors. Precondition: pack has already been validated and x/y/texture/u/v
// are within the corresponding bounds. They intentionally avoid repeated header validation.
inline u8 packed_map_cell4_unchecked(const AssetPackView& pack, i32 x, i32 y) {
    const size_t idx = static_cast<size_t>(y) * pack.map_width() + static_cast<size_t>(x);
    const u8 byte = pack.map4()[idx >> 1];
    return (idx & 1u) ? static_cast<u8>(byte >> 4) : static_cast<u8>(byte & 0x0Fu);
}

inline const u8* texture8_unchecked(const AssetPackView& pack, u8 texture) {
    const size_t map_bytes = static_cast<size_t>(pack.data[10]) | (static_cast<size_t>(pack.data[11]) << 8);
    return pack.data + 16u + map_bytes + static_cast<size_t>(texture) * 64u * 64u;
}

u8 texture_sample64(const AssetPackView& pack, u8 texture, u8 u, u8 v);
inline u8 texture_sample64_unchecked(const AssetPackView& pack, u8 texture, u8 u, u8 v) {
    const u8* t = texture8_unchecked(pack, texture);
    return t[static_cast<size_t>(v & 63u) * 64u + static_cast<size_t>(u & 63u)];
}
// Material IDs are the compact 4-bit values stored in the map. A material maps
// directly to a texture slot when present, otherwise it falls back to texture 0.
u8 material_texture(const AssetPackView& pack, u8 material);

// Material 0 is empty. Material IDs 1..14 map directly to texture slots when present;
// material 15 is reserved for the dynamic door; it uses the regular wall texture until
// a dedicated door-wall material table is introduced, while sprite texture slots stay independent.


// Built-in tiny demonstration pack used as a safe fallback in tests/targets without embedded content.
AssetPackView builtin_assets();

// Build-generated asset pack. The desktop build embeds assets/map.txt + assets/textures.raw
// into flash/rodata and binds the resulting immutable view without heap allocation.
AssetPackView generated_assets();

} // namespace l3d
