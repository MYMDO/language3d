#include "api.h"
#include "game.h"
#include <new>
#include <cstdint>
#include "resource_cache.h"
#include "world_grid.h"

namespace {
struct EngineInstance {
    l3d::Game game;
    alignas(16) l3d::u16 depth[l3d::Config::WIDTH]{};
    l3d::SoftwareRaycaster renderer;
    l3d::AssetPackView assets;
    l3d::WorldGridView world_grid;
    l3d::ResourceCache<8> resources;
    l3d::SimulationClock clock;
};
static EngineInstance* as_engine(void* p) { return static_cast<EngineInstance*>(p); }
}

extern "C" size_t l3d_engine_size(void) { return sizeof(EngineInstance); }
extern "C" size_t l3d_engine_align(void) { return alignof(EngineInstance); }
extern "C" int l3d_engine_init(void* memory, size_t memory_size) {
    if (!memory || memory_size < sizeof(EngineInstance)) return 0;
    if (reinterpret_cast<std::uintptr_t>(memory) % alignof(EngineInstance) != 0) return 0;
    new (memory) EngineInstance{};
    as_engine(memory)->assets = l3d::builtin_assets();
    as_engine(memory)->game.set_assets(as_engine(memory)->assets);
    as_engine(memory)->renderer.set_assets(as_engine(memory)->assets);
    as_engine(memory)->world_grid.init(as_engine(memory)->assets);
    as_engine(memory)->renderer.set_world(&as_engine(memory)->world_grid);
    as_engine(memory)->game.reset();
    as_engine(memory)->clock.accumulator_ms = 0;
    return 1;
}
extern "C" void l3d_engine_reset(void* memory) { if (memory) { as_engine(memory)->game.reset(); as_engine(memory)->clock.accumulator_ms = 0; } }
extern "C" int l3d_engine_set_assets(void* memory, const void* data, size_t size) {
    if (!memory || !data || !l3d::asset_pack_validate(static_cast<const l3d::u8*>(data), size)) return 0;
    as_engine(memory)->assets = {static_cast<const l3d::u8*>(data), size};
    as_engine(memory)->game.set_assets(as_engine(memory)->assets);
    as_engine(memory)->renderer.set_assets(as_engine(memory)->assets);
    as_engine(memory)->world_grid.init(as_engine(memory)->assets);
    as_engine(memory)->renderer.set_world(&as_engine(memory)->world_grid);
    as_engine(memory)->game.reset();
    as_engine(memory)->clock.accumulator_ms = 0;
    return 1;
}
extern "C" void l3d_engine_update_ms(void* memory, const l3d_input* in, uint32_t dt_ms) {
    if (!memory || !in) return;
    l3d::InputState x{};
    x.up=in->up!=0; x.down=in->down!=0; x.left=in->left!=0; x.right=in->right!=0;
    x.strafe_left=in->strafe_left!=0; x.strafe_right=in->strafe_right!=0;
    x.mouse_x=in->mouse_x;
    x.interact_pressed=in->interact_pressed!=0; x.language_pressed=in->language_pressed!=0;
    x.progress_pressed=in->progress_pressed!=0; x.help_pressed=in->help_pressed!=0; x.escape_pressed=in->escape_pressed!=0;
    x.answer=in->answer;
    as_engine(memory)->clock.advance(dt_ms, [&](l3d::u32 tick_ms) { as_engine(memory)->game.update(x, tick_ms); });
}
extern "C" int l3d_engine_render(void* memory, l3d_framebuffer8* out) {
    if (!memory || !out || !out->pixels) return 0;
    if (out->width != l3d::Config::WIDTH || out->height != l3d::Config::HEIGHT || out->stride < l3d::Config::WIDTH) return 0;
    l3d::Framebuffer8 fb{out->pixels, out->width, out->height, out->stride};
    fb.column_depth = as_engine(memory)->depth;
    as_engine(memory)->game.render(as_engine(memory)->renderer, fb);
    return 1;
}
extern "C" uint16_t l3d_width(void) { return l3d::Config::WIDTH; }
extern "C" uint16_t l3d_height(void) { return l3d::Config::HEIGHT; }
extern "C" size_t l3d_framebuffer_bytes(void) { return l3d::MemoryBudget::FRAMEBUFFER8; }
extern "C" size_t l3d_reserved_sram_bytes(void) { return l3d::MemoryBudget::TOTAL_RESERVED; }
extern "C" size_t l3d_sram_headroom_bytes(void) { return l3d::MemoryBudget::SRAM_BYTES - l3d::MemoryBudget::TOTAL_RESERVED; }
extern "C" size_t l3d_resource_cache_slots(void) { return 8u; }

extern "C" size_t l3d_render_stats_size(void) { return sizeof(l3d::RenderStats); }
extern "C" int l3d_get_render_stats(void* memory, void* out_stats, size_t out_size) {
    if (!memory || !out_stats || out_size < sizeof(l3d::RenderStats)) return 0;
    *static_cast<l3d::RenderStats*>(out_stats) = as_engine(memory)->renderer.stats();
    return 1;
}
