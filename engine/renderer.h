#pragma once
#include "core.h"
#include "assets.h"
#include "entities.h"
#include "world.h"
#include "world_grid.h"
#include "capabilities.h"
#include <array>
#include <string_view>

namespace l3d {

struct Palette {
    std::array<u32, 256> argb{};
    std::array<u16, 256> rgb565{};
    void make_default();
};

struct RenderStats {
    u32 frame_id{0};
    u16 rays{0};
    u16 dda_steps{0};
    u32 wall_pixels{0};
    u16 sprites_considered{0};
    u16 sprites_drawn{0};
    u32 sprite_pixels{0};
    u32 floor_pixels{0};
};

struct Framebuffer8 {
    u8* pixels{nullptr};
    u16 width{0};
    u16 height{0};
    u32 stride{0};
    u16* column_depth{nullptr};
    bool valid() const { return pixels && width >= Config::WIDTH && height >= Config::HEIGHT && stride >= Config::WIDTH; }
    bool depth_valid() const { return column_depth != nullptr; }
    void clear(u8 index);
    void rect(i32 x, i32 y, i32 w, i32 h, u8 index);
};

// The engine exposes a concrete software renderer. There is deliberately no
// virtual dispatch in the frame hot path: the Game asks for this exact type.
// Higher-level platform code may select another backend at compile time.
class SoftwareRaycaster final {
public:
    void set_assets(const AssetPackView& assets) { assets_ = assets; }
    void set_doors(const DoorSystem<8>* doors) { doors_ = doors; }
    void set_world(const WorldGridView* world) { world_grid_ = world; }
    constexpr RendererCapabilities capabilities() const { return software_capabilities(); }
    void begin(Framebuffer8& fb);
    void draw_world(Framebuffer8& fb, const Player& player, const NPC& npc);
    void draw_sprites(Framebuffer8& fb, const Player& player, SpriteEntity* sprites, size_t count);
    void draw_hud(Framebuffer8& fb, std::string_view objective, std::string_view status);
    void draw_mode(Framebuffer8& fb, Mode mode, const Progress& progress);
    void draw_debug_camera(Framebuffer8& fb, const Player& player);
    Vec2 center_ray(u16 angle_turn) const;
    i32 project_world_x(const Player& player, const Vec2& point) const;
    void end(Framebuffer8&) {}
    const Palette& palette() const { return palette_; }
    const RenderStats& stats() const { return stats_; }
private:
    Palette palette_{};
    AssetPackView assets_{};
    const DoorSystem<8>* doors_{nullptr};
    const WorldGridView* world_grid_{nullptr};
    RenderStats stats_{};
};

}
