#include "renderer.h"
#include "game.h"
#include "assets.h"
#include "l3d_math.h"
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <cstring>


namespace l3d {
namespace {
constexpr u8 SKY=16, FLOOR=24, WALL=80, WALL_DARK=64, NPC_COLOR=140, PANEL=32, ACCENT=200;
constexpr u8 SPRITE_TRANSPARENT=0;
constexpr u8 SPRITE_TEX=1;
constexpr i32 FOV_PLANE_RAW = 42562; // tan(33 deg) in 16.16 ~= 0.6494

// Exact pixel-center camera coordinates in Q16.16. Keeping the table constexpr
// avoids a per-column division while removing the old 411/409.6 approximation
// that accumulated a small FOV skew across the screen.
constexpr std::array<i32, Config::WIDTH> make_camera_x_table() {
    std::array<i32, Config::WIDTH> t{};
    for (size_t x = 0; x < Config::WIDTH; ++x) {
        // Pixel-center coordinate in Q16.16: ((2*x + 1) - WIDTH) / WIDTH.
        // The previous implementation accidentally multiplied the pixel term by
        // 2, making the middle column use camera_x ~= +1 instead of ~0.
        const i64 num = (i64(2) * i64(x) + 1 - i64(Config::WIDTH)) * 65536ll;
        t[x] = static_cast<i32>((num + (num >= 0 ? (i64(Config::WIDTH)/2) : -(i64(Config::WIDTH)/2))) / i64(Config::WIDTH));
    }
    return t;
}
constexpr auto CAMERA_X_Q16 = make_camera_x_table();

inline i32 abs_i32(i32 v) { return v < 0 ? -v : v; }

// Reciprocal lookup for the hot DDA/projection path. The old table covered only
// 0..2.0 and therefore saturated any wall farther than two map units to the
// reciprocal of 2.0, producing oversized/incorrect walls. v21 normalizes the
// Q16.16 input into [1.0, 2.0) and uses a small 257-entry table plus a power-of-two
// exponent adjustment. This covers the full positive i32 range without a large LUT.
constexpr size_t RECIP_COUNT = 257;
constexpr std::array<i32, RECIP_COUNT> make_reciprocal_table() {
    std::array<i32, RECIP_COUNT> t{};
    for (size_t i = 0; i < RECIP_COUNT; ++i) {
        const i64 norm = 65536ll + i64(i) * 256ll;
        t[i] = static_cast<i32>((i64(1) << 32) / norm);
    }
    return t;
}
constexpr auto RECIP_Q16 = make_reciprocal_table();

inline Fx inv_abs(Fx v) {
    const u32 a = static_cast<u32>(abs_i32(v.raw));
    if (a == 0u) return Fx::from_raw(0x7FFFFFFF);

    u32 norm = a;
    i32 shift = 0;
    while (norm >= 131072u) {
        norm >>= 1;
        ++shift;
    }
    while (norm < 65536u) {
        norm <<= 1;
        --shift;
    }

    const u32 frac = norm - 65536u;
    const u32 idx = frac >> 8;
    const u32 rem = frac & 0xFFu;
    const i32 a0 = RECIP_Q16[idx];
    const i32 a1 = RECIP_Q16[idx + 1u];
    i64 recip = i64(a0) + ((i64(a1 - a0) * rem) >> 8);

    if (shift > 0) recip >>= shift;
    else if (shift < 0) recip <<= -shift;
    if (recip > 0x7FFFFFFFll) recip = 0x7FFFFFFFll;
    if (recip < 0) recip = 0;
    return Fx::from_raw(static_cast<i32>(recip));
}

inline i32 mul_raw(i32 a, i32 b) {
    return static_cast<i32>((i64(a) * i64(b)) >> 16);
}

inline i32 clamp_i32(i32 v, i32 lo, i32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

void Palette::make_default() {
    for (size_t i=0;i<argb.size();++i) {
        const u8 v=static_cast<u8>(i);
        argb[i]=0xFF000000u|(u32(v)<<16)|(u32(v)<<8)|v;
    }
    argb[SKY]=0xFF6FA8DC; argb[FLOOR]=0xFF2F3542;
    argb[WALL]=0xFF73808F; argb[WALL_DARK]=0xFF4E5863;
    argb[NPC_COLOR]=0xFFE7B86A; argb[PANEL]=0xFF1B2230; argb[ACCENT]=0xFF5DE0E6;
    for (size_t i = 0; i < rgb565.size(); ++i) {
        const u32 c = argb[i];
        const u32 r = (c >> 16) & 0xFFu;
        const u32 g = (c >> 8) & 0xFFu;
        const u32 b = c & 0xFFu;
        rgb565[i] = static_cast<u16>(((r * 31u + 127u) / 255u) << 11 |
                                      ((g * 63u + 127u) / 255u) << 5 |
                                      ((b * 31u + 127u) / 255u));
    }
}

void Framebuffer8::clear(u8 c) {
    if (!valid()) return;
    for (i32 y=0;y<Config::HEIGHT;++y)
        std::memset(pixels + static_cast<size_t>(y)*stride, c, Config::WIDTH);
}

void Framebuffer8::rect(i32 x,i32 y,i32 w,i32 h,u8 c) {
    if (!valid() || w<=0 || h<=0) return;
    const i32 x0=std::max<i32>(0,x), y0=std::max<i32>(0,y);
    const i32 x1=std::min<i32>(Config::WIDTH,x+w), y1=std::min<i32>(Config::HEIGHT,y+h);
    for (i32 yy=y0;yy<y1;++yy)
        for (i32 xx=x0;xx<x1;++xx)
            pixels[static_cast<size_t>(yy)*stride+xx]=c;
}

void SoftwareRaycaster::begin(Framebuffer8& fb) {
    ++stats_.frame_id;
    stats_.rays = 0;
    stats_.dda_steps = 0;
    stats_.wall_pixels = 0;
    stats_.sprites_considered = 0;
    stats_.sprites_drawn = 0;
    stats_.sprite_pixels = 0;
    stats_.floor_pixels = 0;
    if (!fb.valid()) return;
    if (palette_.argb[SKY]==0) palette_.make_default();
    fb.clear(SKY);
    if (fb.column_depth) {
        for (i32 x = 0; x < Config::WIDTH; ++x) fb.column_depth[x] = 0xFFFFu;
    }
}


namespace {
void draw_one_sprite(Framebuffer8& fb, const Fx dirX, const Fx dirY, const Vec2& playerPos,
                     const SpriteEntity& sprite, const AssetPackView& assets, RenderStats* stats) {
    if (!fb.valid()) return;
    const i32 relX = sprite.pos.x.raw - playerPos.x.raw;
    const i32 relY = sprite.pos.y.raw - playerPos.y.raw;
    const i32 forward = mul_raw(relX, dirX.raw) + mul_raw(relY, dirY.raw);
    const i32 lateral = mul_raw(relX, -dirY.raw) + mul_raw(relY, dirX.raw);
    if (forward <= 0) return;

    const i32 screenOffset = static_cast<i32>((i64(lateral) * Config::WIDTH) /
                                              std::max<i32>(1, forward * 2));
    const i32 cx = Config::WIDTH / 2 + screenOffset;
    const i32 baseSize = clamp_i32(static_cast<i32>((i64(Config::HEIGHT) * 32768) /
                                                     std::max<i32>(1, forward)),
                                   4, Config::HEIGHT * 2);
    const i32 width = std::max<i32>(2, static_cast<i32>((i64(baseSize) * i64(std::max<u8>(8, sprite.width_scale))) / 64));
    const i32 height = std::max<i32>(2, static_cast<i32>((i64(baseSize) * i64(std::max<u8>(8, sprite.height_scale))) / 64));
    const i32 left = cx - width / 2;
    const i32 top = Config::HEIGHT / 2 - height / 2;
    const i32 right = left + width;
    const i32 bottom = top + height;
    if (right <= 0 || left >= Config::WIDTH || bottom <= 0 || top >= Config::HEIGHT) return;

    const u8 tex = (sprite.texture < assets.texture_count()) ? sprite.texture : 0;
    const u8* texData = texture8_unchecked(assets, tex);
    if (!texData) return;
    const i32 spriteDepth8 = clamp_i32(forward >> 8, 0, 65535);
    const i32 x0 = std::max<i32>(0, left), x1 = std::min<i32>(Config::WIDTH, right);
    const i32 y0 = std::max<i32>(0, top), y1 = std::min<i32>(Config::HEIGHT, bottom);
    // Incremental texture coordinates: no integer division in the inner pixel loops.
    const i32 du = (64 << 16) / std::max<i32>(1, width);
    const i32 dv = (64 << 16) / std::max<i32>(1, height);
    i32 v_acc = (y0 - top) * dv;
    for (i32 y = y0; y < y1; ++y) {
        const u8 sv = static_cast<u8>(clamp_i32(v_acc >> 16, 0, 63));
        i32 u_acc = (x0 - left) * du;
        const u8* srcRow = texData + static_cast<size_t>(sv) * 64u;
        u8* dst = fb.pixels + static_cast<size_t>(y) * fb.stride + x0;
        for (i32 x = x0; x < x1; ++x) {
            if (fb.column_depth) {
                const u16 wall8 = fb.column_depth[x];
                if (wall8 != 0xFFFFu && spriteDepth8 > i32(wall8)) {
                    u_acc += du;
                    continue;
                }
            }
            const u8 su = static_cast<u8>(clamp_i32(u_acc >> 16, 0, 63));
            const u8 pixel = srcRow[su];
            if (pixel != SPRITE_TRANSPARENT) {
                dst[x - x0] = pixel;
                if (stats) ++stats->sprite_pixels;
            }
            u_acc += du;
        }
        v_acc += dv;
    }
}
} // namespace

void SoftwareRaycaster::draw_world(Framebuffer8& fb,const Player& p,const NPC&) {
    if (!fb.valid()) return;

    // Ground/floor is rendered as horizontal spans. The renderer can later swap
    // this flat span path for a textured floor without changing the framebuffer ABI.
    const i32 horizon = Config::HEIGHT / 2;
    for (i32 y=horizon; y<Config::HEIGHT; ++y) {
        u8* row = fb.pixels + static_cast<size_t>(y) * fb.stride;
        std::fill(row, row + Config::WIDTH, FLOOR);
        stats_.floor_pixels += Config::WIDTH;
    }

    const Fx dirX=TrigLut::cos16(p.angle_turn);
    const Fx dirY=TrigLut::sin16(p.angle_turn);
    // Camera plane is perpendicular to view direction and scaled by tan(FOV/2).
    const Fx planeX=Fx::from_raw(-mul_raw(dirY.raw,FOV_PLANE_RAW));
    const Fx planeY=Fx::from_raw( mul_raw(dirX.raw,FOV_PLANE_RAW));

    const i32 posX=p.pos.x.raw, posY=p.pos.y.raw;
    const i32 mapX0=posX>>16, mapY0=posY>>16;
    Fx rayX=Fx::from_raw(dirX.raw + mul_raw(planeX.raw,CAMERA_X_Q16[0]));
    Fx rayY=Fx::from_raw(dirY.raw + mul_raw(planeY.raw,CAMERA_X_Q16[0]));
    const i32 cameraDeltaRaw = CAMERA_X_Q16[1] - CAMERA_X_Q16[0];
    const i32 rayStepX = mul_raw(planeX.raw,cameraDeltaRaw);
    const i32 rayStepY = mul_raw(planeY.raw,cameraDeltaRaw);

    const AssetPackView world = assets_.valid() ? assets_ : builtin_assets();
    WorldGridView local_grid{};
    const WorldGridView* grid = world_grid_;
    if (!grid || !grid->valid() || grid->pack != &world) {
        local_grid.init(world);
        grid = &local_grid;
    }
    const auto map_cell = [grid](i32 x, i32 y) -> u8 { return grid->cell(x, y); };
    for (i32 sx=0;sx<Config::WIDTH;++sx) {
        ++stats_.rays;

        i32 mapX=mapX0, mapY=mapY0;
        const i32 stepX = rayX.raw < 0 ? -1 : 1;
        const i32 stepY = rayY.raw < 0 ? -1 : 1;
        const Fx deltaX=inv_abs(rayX), deltaY=inv_abs(rayY);

        const i32 cellX = posX & 0xFFFF;
        const i32 cellY = posY & 0xFFFF;
        Fx sideX = rayX.raw < 0
            ? Fx::from_raw(mul_raw(cellX, deltaX.raw))
            : Fx::from_raw(mul_raw(65536-cellX, deltaX.raw));
        Fx sideY = rayY.raw < 0
            ? Fx::from_raw(mul_raw(cellY, deltaY.raw))
            : Fx::from_raw(mul_raw(65536-cellY, deltaY.raw));

        i32 side=0;
        Fx perp=Fx::from_int(1);
        bool hit=false;
        for (i32 depth=0; depth<64; ++depth) {
            ++stats_.dda_steps;
            if (sideX.raw < sideY.raw) {
                sideX.raw += deltaX.raw;
                mapX += stepX;
                side=0;
            } else {
                sideY.raw += deltaY.raw;
                mapY += stepY;
                side=1;
            }
            const u8 material = map_cell(mapX, mapY);
            const bool blocking = (material != 0) && !(material == DoorSystem<8>::DOOR_MATERIAL && doors_ && doors_->is_fully_open(mapX, mapY));
            if (blocking) {
                perp = side==0 ? Fx::from_raw(sideX.raw-deltaX.raw)
                               : Fx::from_raw(sideY.raw-deltaY.raw);
                hit=true;
                break;
            }
        }
        if (!hit || perp.raw <= 0) {
            rayX.raw += rayStepX;
            rayY.raw += rayStepY;
            continue;
        }

        // Preserve the projected wall height in Q16.16 all the way through
        // vertical sampling. Quantizing the height to integer pixels before
        // computing tex-V caused adjacent screen columns to restart the
        // texture at slightly different phases, producing sawtooth horizontal
        // wall lines at higher resolutions.
        const i32 inv_perp_raw = inv_abs(perp).raw;
        const i32 min_line_raw = 1 << 16;
        const i32 max_line_raw = Config::HEIGHT * 4 << 16;
        const i64 line_raw_unclamped = i64(Config::HEIGHT) * i64(inv_perp_raw);
        const i64 line_raw_limited = std::max<i64>(min_line_raw, std::min<i64>(max_line_raw, line_raw_unclamped));
        const i32 line_raw = static_cast<i32>(line_raw_limited);
        const i32 half_h_raw = Config::HEIGHT << 15;
        const i32 top_raw = half_h_raw - line_raw / 2;
        const i32 bot_raw = half_h_raw + line_raw / 2;
        const i32 top = top_raw >= 0 ? ((top_raw + 65535) >> 16) : (top_raw >> 16);
        const i32 bot = bot_raw >= 0 ? ((bot_raw + 65535) >> 16) : (bot_raw >> 16);
        // Store wall depth for billboard occlusion. Saturate to 16-bit 1/256 world units.
        if (fb.column_depth) {
            const i32 depth8 = clamp_i32(perp.raw >> 8, 0, 65535);
            fb.column_depth[sx] = static_cast<u16>(depth8);
        }

        // Textured column: one 64x64 indexed texture shared by all wall cells in the
        // built-in pack. Keep the calculation integer/fixed-point and avoid per-pixel RGB math.
        const i32 hitX = posX + static_cast<i32>((i64(rayX.raw) * perp.raw) >> 16);
        const i32 hitY = posY + static_cast<i32>((i64(rayY.raw) * perp.raw) >> 16);
        const i32 fracRaw = side ? (hitX & 0xFFFF) : (hitY & 0xFFFF);
        const u8 hitMaterial = map_cell(mapX, mapY);
        const u8 texU = static_cast<u8>((static_cast<u32>(fracRaw) * 64u) >> 16);
        const i32 y0 = std::max<i32>(0, top);
        const i32 y1 = std::min<i32>(Config::HEIGHT, bot);
        // Procedural wall material: the panel grid is derived from continuous
        // wall coordinates instead of sampling a 64×64 image. This avoids
        // texel-phase discontinuities while keeping the visual language cheap.
        // texVStep is Q16.16 texels/pixel derived from the exact projected
        // wall height. Use the pixel center for the initial sample so the
        // fractional top edge is preserved instead of being discarded.
        const i32 texVStep = static_cast<i32>(std::min<i64>(
            i64(0x7FFFFFFF), i64(inv_abs(Fx::from_raw(line_raw)).raw) * 64ll));
        const i64 first_center_raw = i64(y0) * 65536ll + 32768ll;
        const i64 first_v_raw = ((first_center_raw - i64(top_raw)) * i64(texVStep)) >> 16;
        i32 texVAcc = static_cast<i32>(clamp_i32(
            first_v_raw < 0 ? 0 : (first_v_raw > 0x7FFFFFFFll ? 0x7FFFFFFF : static_cast<i32>(first_v_raw)),
            0, 0x7FFFFFFF));
        const u8 panel_x = static_cast<u8>(texU >> 3);
        const i32 shade = clamp_i32(127 - (perp.raw >> 17), 24, 127);

        for (i32 y=y0;y<y1;++y) {
            const u8 texV = static_cast<u8>(clamp_i32(texVAcc >> 16,0,63));
            // Fill the panel colors from the continuous vertical coordinate.
            // Horizontal seams are overlaid below from their exact projected
            // positions rather than using a one-texel-thick nearest-neighbor seam.
            const u8 panel_y = static_cast<u8>(texV >> 3);
            (void)panel_y;
            // Horizontal boundaries are rendered independently as geometric seams.
            // Keep panel fill independent of panel_y so small sub-pixel projection
            // differences cannot turn the boundary into triangular checkerboard teeth.
            const u8 palette_base = ((panel_x + hitMaterial) & 1u) ? WALL_DARK : WALL;
            u8 idx = palette_base;
            idx = static_cast<u8>((i32(idx) * shade) >> 7);
            if (side) idx = static_cast<u8>(idx >> 1);
            fb.pixels[static_cast<size_t>(y)*fb.stride+sx]=idx;
            ++stats_.wall_pixels;
            texVAcc += texVStep;
        }

        // Draw seven horizontal panel seams from exact Q16.16 projected positions.
        // A single nearest scanline makes the seam a screen-space geometric feature
        // instead of a 1-texel texture row. The result has only normal 1-pixel
        // rasterization steps rather than the large repeating teeth from nearest
        // vertical texture sampling.
        const i32 seam_shaded = static_cast<i32>((i32(PANEL) * shade) >> 7);
        for (i32 k = 1; k < 8; ++k) {
            const i64 seam_raw = i64(top_raw) + (i64(line_raw) * i64(k)) / 8ll;
            const i32 sy = static_cast<i32>((seam_raw + 32768ll) >> 16);
            if (sy >= y0 && sy < y1) {
                fb.pixels[static_cast<size_t>(sy) * fb.stride + sx] = static_cast<u8>(side ? (seam_shaded >> 1) : seam_shaded);
            }
        }
        rayX.raw += rayStepX;
        rayY.raw += rayStepY;
    }

    // Compact sprite batch path. The caller owns the entity array; no heap allocation or
    // sorting buffer is required for the common MCU case.

    // Center reticle.
    const i32 cx=Config::WIDTH/2, cy=Config::HEIGHT/2;
    for (i32 i=-4;i<=4;++i) {
        if (cx+i>=0 && cx+i<Config::WIDTH) {
            fb.pixels[static_cast<size_t>(cy-1)*fb.stride+cx+i]=ACCENT;
            fb.pixels[static_cast<size_t>(cy+1)*fb.stride+cx+i]=ACCENT;
        }
        if (cy+i>=0 && cy+i<Config::HEIGHT)
            fb.pixels[static_cast<size_t>(cy+i)*fb.stride+cx]=ACCENT;
    }
}


Vec2 SoftwareRaycaster::center_ray(u16 angle_turn) const {
    const Fx dirX = TrigLut::cos16(angle_turn);
    const Fx dirY = TrigLut::sin16(angle_turn);
    const Fx planeX = Fx::from_raw(-mul_raw(dirY.raw, FOV_PLANE_RAW));
    const Fx planeY = Fx::from_raw( mul_raw(dirX.raw, FOV_PLANE_RAW));
    const i32 cx = Config::WIDTH / 2;
    const i32 cameraX = CAMERA_X_Q16[static_cast<size_t>(cx)];
    return Vec2{
        Fx::from_raw(dirX.raw + mul_raw(planeX.raw, cameraX)),
        Fx::from_raw(dirY.raw + mul_raw(planeY.raw, cameraX))
    };
}

i32 SoftwareRaycaster::project_world_x(const Player& player, const Vec2& point) const {
    const i64 relX = i64(point.x.raw) - player.pos.x.raw;
    const i64 relY = i64(point.y.raw) - player.pos.y.raw;
    const Fx dirX = TrigLut::cos16(player.angle_turn);
    const Fx dirY = TrigLut::sin16(player.angle_turn);
    const Fx planeX = Fx::from_raw(-mul_raw(dirY.raw, FOV_PLANE_RAW));
    const Fx planeY = Fx::from_raw( mul_raw(dirX.raw, FOV_PLANE_RAW));
    const i64 detQ32 = i64(dirX.raw) * planeY.raw - i64(planeX.raw) * dirY.raw;
    if (detQ32 == 0) return -1;
    // cameraX is Q16.16. Numerator is Q32.32; divide directly by the Q32.32 determinant.
    const i64 numQ32 = (-i64(dirY.raw) * relX) + (i64(dirX.raw) * relY);
    const i64 cameraXQ16 = (numQ32 << 16) / detQ32;
    const i64 screenX = (i64(Config::WIDTH) * (cameraXQ16 + 65536ll)) >> 17;
    if (screenX < -Config::WIDTH || screenX > i64(Config::WIDTH) * 2) return -1;
    return static_cast<i32>(screenX);
}

void SoftwareRaycaster::draw_sprites(Framebuffer8& fb, const Player& player,
                                       SpriteEntity* sprites, size_t count) {
    if (!fb.valid() || !sprites || count == 0) return;
    const AssetPackView assets = assets_.valid() ? assets_ : builtin_assets();
    const size_t n = std::min<size_t>(count, 32);
    stats_.sprites_considered = static_cast<u16>(n);
    u8 order[32]{};
    for (size_t i = 0; i < n; ++i) order[i] = static_cast<u8>(i);
    for (size_t i = 1; i < n; ++i) {
        const u8 key = order[i];
        const SpriteEntity& ks = sprites[key];
        const i64 kdx = i64(ks.pos.x.raw) - player.pos.x.raw;
        const i64 kdy = i64(ks.pos.y.raw) - player.pos.y.raw;
        const i64 kdist = kdx*kdx + kdy*kdy;
        size_t j = i;
        while (j > 0) {
            const SpriteEntity& prev = sprites[order[j-1]];
            const i64 dx = i64(prev.pos.x.raw) - player.pos.x.raw;
            const i64 dy = i64(prev.pos.y.raw) - player.pos.y.raw;
            if (dx*dx + dy*dy >= kdist) break;
            order[j] = order[j-1];
            --j;
        }
        order[j] = key;
    }
    const Fx dirX = TrigLut::cos16(player.angle_turn);
    const Fx dirY = TrigLut::sin16(player.angle_turn);
    for (size_t i = 0; i < n; ++i) {
        const size_t before = stats_.sprite_pixels;
        draw_one_sprite(fb, dirX, dirY, player.pos, sprites[order[i]], assets, &stats_);
        if (stats_.sprite_pixels > before) ++stats_.sprites_drawn;
    }
}

void SoftwareRaycaster::draw_debug_camera(Framebuffer8& fb, const Player& player) {
    if (!fb.valid()) return;
    const i32 cy = Config::HEIGHT / 2;
    const Vec2 ray = center_ray(player.angle_turn);
    const Vec2 probe{
        Fx::from_raw(player.pos.x.raw + ray.x.raw * 8),
        Fx::from_raw(player.pos.y.raw + ray.y.raw * 8)
    };
    const i32 projected = project_world_x(player, probe);
    if (projected < 0 || projected >= Config::WIDTH) return;
    for (i32 y = std::max<i32>(0, cy - 6); y <= std::min<i32>(Config::HEIGHT - 1, cy + 6); ++y) {
        fb.pixels[static_cast<size_t>(y) * fb.stride + projected] = ACCENT;
    }
    const i32 cx = Config::WIDTH / 2;
    for (i32 dx = -3; dx <= 3; ++dx) {
        const i32 x = projected + dx;
        if (x >= 0 && x < Config::WIDTH) fb.pixels[static_cast<size_t>(cy) * fb.stride + x] = ACCENT;
    }
    // Mark the mathematical center of the projection one color index away from
    // the regular reticle: if this marker drifts, forward-vector and camera
    // projection have diverged.
    if (cx >= 0 && cx < Config::WIDTH) {
        fb.pixels[static_cast<size_t>(cy - 8) * fb.stride + cx] = PANEL;
        fb.pixels[static_cast<size_t>(cy + 8) * fb.stride + cx] = PANEL;
    }
}

void SoftwareRaycaster::draw_hud(Framebuffer8& fb,std::string_view,std::string_view) {
    fb.rect(0,0,Config::WIDTH,24,PANEL);
    fb.rect(0,Config::HEIGHT-34,Config::WIDTH,34,PANEL);
}

void SoftwareRaycaster::draw_mode(Framebuffer8& fb,Mode,const Progress&) {
    fb.clear(SKY);
    fb.rect(12,10,Config::WIDTH-24,Config::HEIGHT-20,PANEL);
}

} // namespace l3d
