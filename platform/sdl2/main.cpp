#include "../../engine/game.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <array>
#include <algorithm>
#include <vector>
#include <cstdint>

using namespace l3d;
static size_t linux_rss_kib(){
    std::ifstream f("/proc/self/statm");
    long a=0,b=0;
    if(f>>a>>b) return size_t(b*4096/1024);
    return 0;
}

static size_t linux_hwm_kib(){
    std::ifstream f("/proc/self/status");
    std::string key;
    size_t value=0;
    std::string unit;
    while(f>>key>>value>>unit){
        if(key=="VmHWM:") return value; // /proc reports KiB here
    }
    return 0;
}

static constexpr size_t embedded_reserved_bytes(size_t width, size_t height, size_t buffers){
    const size_t framebuffer8 = width * height;
    const size_t display_fb8 = framebuffer8 * buffers;
    const size_t depth16 = width * sizeof(u16);
    return display_fb8
        + depth16
        + MemoryBudget::ENGINE_STATE
        + MemoryBudget::ENTITY_POOL
        + MemoryBudget::RESOURCE_WORK
        + MemoryBudget::AUDIO
        + MemoryBudget::COMMANDS
        + MemoryBudget::RESOURCE_CACHE
        + MemoryBudget::STACK_RESERVE;
}

static void print_memory_breakdown(){
    const double kib = 1024.0;
    const auto fb_single = MemoryBudget::FRAMEBUFFER8;
    const auto fb_total = MemoryBudget::DISPLAY_FRAMEBUFFER8;
    const auto depth = MemoryBudget::DEPTH16;
    const auto engine = MemoryBudget::ENGINE_STATE;
    const auto entities = MemoryBudget::ENTITY_POOL;
    const auto resource = MemoryBudget::RESOURCE_WORK;
    const auto audio = MemoryBudget::AUDIO;
    const auto commands = MemoryBudget::COMMANDS;
    const auto cache = MemoryBudget::RESOURCE_CACHE;
    const auto stack = MemoryBudget::STACK_RESERVE;
    const auto embedded_double = embedded_reserved_bytes(240u, 240u, 2u);
    const auto embedded_single = embedded_reserved_bytes(240u, 240u, 1u);
    std::cerr << "[MEM] current profile reservation breakdown (KiB)\n"
              << "[MEM]   framebuffer8 single=" << std::fixed << std::setprecision(2) << fb_single/kib
              << " total=" << fb_total/kib << " (" << MemoryBudget::DISPLAY_FRAME_BUFFERS << " buffers)\n"
              << "[MEM]   depth16=" << depth/kib
              << " | engine=" << engine/kib
              << " | entities=" << entities/kib
              << " | resource-work=" << resource/kib
              << " | audio=" << audio/kib << "\n"
              << "[MEM]   commands=" << commands/kib
              << " | resource-cache=" << cache/kib
              << " | stack-reserve=" << stack/kib << "\n"
              << "[MEM]   TOTAL=" << MemoryBudget::TOTAL_RESERVED/kib << " KiB\n"
              << "[MEM]   Embedded model @ 240x240: double-FB=" << embedded_double/kib
              << " KiB | single-FB=" << embedded_single/kib
              << " KiB\n"
              << "[MEM]   RP2040 (264 KiB): single-FB headroom="
              << ((264u*1024u > embedded_single) ? (264u*1024u-embedded_single)/kib : 0.0) << " KiB\n"
              << "[MEM]   RP2350 (520 KiB): single-FB headroom="
              << ((520u*1024u > embedded_single) ? (520u*1024u-embedded_single)/kib : 0.0) << " KiB\n"
              << "[MEM]   NOTE: resource-work/audio/commands/stack are deterministic upper bounds, not measured live usage.\n";
}
int main(int argc, char* argv[]){
    (void)argc; (void)argv; // SDL2 entry-point signature; no CLI args used.
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0){ std::fprintf(stderr,"SDL_Init failed: %s\n",SDL_GetError()); return 1; }
    constexpr int TARGET_W = 1920;
    constexpr int TARGET_H = 1080;
    SDL_Window* w=SDL_CreateWindow("Language 3D MVP — PC Linux",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,TARGET_W,TARGET_H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!w){ std::fprintf(stderr,"SDL window failed: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer* r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC); if(!r) r=SDL_CreateRenderer(w,-1,SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_SOFTWARE);
    // Render directly to an explicit aspect-preserving destination rectangle.
    // Do not use SDL logical-size scaling here: the desktop window may have a
    // different client-area aspect ratio because of window decorations or resizing.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_Texture* t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,Config::WIDTH,Config::HEIGHT); if(!t){ std::fprintf(stderr,"texture failed: %s\n",SDL_GetError()); return 1; }
    // Large desktop buffers live on the heap, never on the thread stack. The engine
    // itself remains a caller-owned indexed8 framebuffer, preserving the portable ABI.
    std::vector<u8> pixels(MemoryBudget::FRAMEBUFFER8);
    Framebuffer8 fb{pixels.data(), Config::WIDTH, Config::HEIGHT, Config::WIDTH};
    SoftwareRaycaster renderer;
    Game game;
    const AssetPackView game_assets = generated_assets();
    if (!game_assets.valid()) {
        std::fprintf(stderr, "generated asset pack invalid; refusing to run with mismatched content\n");
        SDL_DestroyTexture(t); SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
        return 1;
    }
    game.set_assets(game_assets);
    std::cerr << "[ASSET] map=" << game_assets.map_width() << "x" << game_assets.map_height()
              << " | textures=" << unsigned(game_assets.texture_count())
              << " | bytes=" << game_assets.size << "\n";
    game.reset();

    SimulationClock sim_clock{};
    bool run=true; uint32_t last=SDL_GetTicks(),stat=last; uint64_t frames=0, sim_ticks=0;
    bool debug_camera=false;
    bool mouse_look=false;
#if defined(L3D_PC_PROFILE)
    std::cerr << "[ARCH] profile=PC-LINUX | internal=" << Config::WIDTH << "x" << Config::HEIGHT
              << " | present=1920x1080 | core=portable fixed16.16 | framebuffer=8-bit indexed\n"
              << "[ARCH] reserved=" << (MemoryBudget::TOTAL_RESERVED/1024.0)
              << " KiB | HOST-RAM-BUDGET=16384 MiB | internal framebuffer is software-rendered at native 1920x1080 (no upscale)\n";
    print_memory_breakdown();
#else
    std::cerr << "[ARCH] profile=EMBEDDED | core=portable fixed16.16 | framebuffer=8-bit indexed | reserved="
              << (MemoryBudget::TOTAL_RESERVED/1024.0) << " KiB / 520 KiB | headroom="
              << ((MemoryBudget::SRAM_BYTES-MemoryBudget::TOTAL_RESERVED)/1024.0) << " KiB\n";
#endif
    // Desktop input latch: held state is driven by KEYDOWN/KEYUP events, while the
    // SDL keyboard snapshot is retained as a recovery path for backends that may
    // drop an event during focus transitions. One-shot actions persist until a
    // simulation tick consumes them.
    struct InputLatch {
        bool up=false, down=false, left=false, right=false;
        bool strafe_left=false, strafe_right=false;
        bool interact=false, language=false, progress=false, help=false, escape=false;
        u8 answer=0;
    } latch;
    i16 latch_mouse_x = 0;

    while(run){
        uint32_t now=SDL_GetTicks(); uint32_t frame_ms=now-last; last=now;
        InputState in{};
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) { run=false; continue; }
            if(e.type==SDL_WINDOWEVENT){
                if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){
                    latch.up=latch.down=latch.left=latch.right=false;
                    latch.strafe_left=latch.strafe_right=false;
                    if(mouse_look){ SDL_SetRelativeMouseMode(SDL_FALSE); mouse_look=false; }
                }
            }
            if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                SDL_SetRelativeMouseMode(SDL_TRUE);
                mouse_look=true;
                SDL_ShowCursor(SDL_DISABLE);
            }
            if(e.type==SDL_MOUSEMOTION && mouse_look){
                const int rel = std::clamp(e.motion.xrel, -2048, 2048);
                const i32 accum = i32(latch_mouse_x) + i32(rel);
                latch_mouse_x = static_cast<i16>(std::clamp(accum, -32768, 32767));
            }
            if(e.type==SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode==SDL_SCANCODE_ESCAPE && mouse_look){
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mouse_look=false;
                SDL_ShowCursor(SDL_ENABLE);
                continue;
            }
            if(e.type==SDL_KEYDOWN || e.type==SDL_KEYUP){
                const bool down = (e.type==SDL_KEYDOWN);
                const SDL_Scancode sc = e.key.keysym.scancode;
                switch(sc){
                    case SDL_SCANCODE_W: latch.up=down; break;
                    case SDL_SCANCODE_S: latch.down=down; break;
                    case SDL_SCANCODE_A: latch.strafe_left=down; break;
                    case SDL_SCANCODE_D: latch.strafe_right=down; break;
                    case SDL_SCANCODE_LEFT: latch.left=down; break;
                    case SDL_SCANCODE_RIGHT: latch.right=down; break;
                    case SDL_SCANCODE_UP: latch.up=down; break;
                    case SDL_SCANCODE_DOWN: latch.down=down; break;
                    case SDL_SCANCODE_Q: latch.strafe_left=down; break;
                    case SDL_SCANCODE_C: latch.strafe_right=down; break;
                    default: break;
                }
                if(down && !e.key.repeat){
                    switch(sc){
                        case SDL_SCANCODE_E: latch.interact=true; break;
                        case SDL_SCANCODE_L: latch.language=true; break;
                        case SDL_SCANCODE_P: latch.progress=true; break;
                        case SDL_SCANCODE_H: latch.help=true; break;
                        case SDL_SCANCODE_ESCAPE: latch.escape=true; break;
                        case SDL_SCANCODE_1: latch.answer=1; break;
                        case SDL_SCANCODE_2: latch.answer=2; break;
                        case SDL_SCANCODE_3: latch.answer=3; break;
                        case SDL_SCANCODE_F3: debug_camera = !debug_camera; break;
                        case SDL_SCANCODE_F11:
                            {
                                const Uint32 flags = SDL_GetWindowFlags(w);
                                SDL_SetWindowFullscreen(w, (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                            }
                            break;
                        default: break;
                    }
                }
            }
        }

        SDL_PumpEvents();
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        in.up    = latch.up    || ks[SDL_SCANCODE_W] || ks[SDL_SCANCODE_UP];
        in.down  = latch.down  || ks[SDL_SCANCODE_S] || ks[SDL_SCANCODE_DOWN];
        in.left  = latch.left  || ks[SDL_SCANCODE_LEFT];
        in.right = latch.right || ks[SDL_SCANCODE_RIGHT];
        in.strafe_left  = latch.strafe_left  || ks[SDL_SCANCODE_A] || ks[SDL_SCANCODE_Q];
        in.strafe_right = latch.strafe_right || ks[SDL_SCANCODE_D] || ks[SDL_SCANCODE_C];
        in.interact_pressed = latch.interact;
        in.language_pressed = latch.language;
        in.progress_pressed = latch.progress;
        in.help_pressed = latch.help;
        in.escape_pressed = latch.escape;
        in.answer = latch.answer;
        in.mouse_x = mouse_look ? latch_mouse_x : 0;

        bool consumed_one_shot = false;
        bool consumed_mouse = false;
        sim_clock.advance(frame_ms, [&](u8 tick_ms){
            InputState tick_input = in;
            if(consumed_mouse) tick_input.mouse_x = 0;
            if(consumed_one_shot){
                tick_input.interact_pressed = false;
                tick_input.language_pressed = false;
                tick_input.progress_pressed = false;
                tick_input.help_pressed = false;
                tick_input.escape_pressed = false;
                tick_input.answer = 0;
            }
            game.update(tick_input, tick_ms);
            consumed_mouse = true;
            consumed_one_shot = true;
            ++sim_ticks;
        });
        // Keep one-shot actions latched if no simulation tick occurred this frame.
        if(consumed_one_shot){
            latch.interact = latch.language = latch.progress = latch.help = latch.escape = false;
            latch.answer = 0;
            if (consumed_mouse) latch_mouse_x = 0;
        }
        game.render(renderer,fb);
        if (debug_camera) renderer.draw_debug_camera(fb, game.player());

        // Convert indexed8 directly into the streaming RGB565 texture. This removes
        // the previous ~8 MiB ARGB8888 staging buffer and avoids a second full-frame
        // copy. Palette conversion is a single LUT lookup per pixel.
        void* tex_pixels = nullptr;
        int tex_pitch = 0;
        if (SDL_LockTexture(t, nullptr, &tex_pixels, &tex_pitch) != 0) {
            std::fprintf(stderr, "texture lock failed: %s\n", SDL_GetError());
            break;
        }
        const auto& lut = renderer.palette().rgb565;
        for (u16 y = 0; y < Config::HEIGHT; ++y) {
            const u8* src = fb.pixels + static_cast<size_t>(y) * fb.stride;
            u16* dst = reinterpret_cast<u16*>(static_cast<u8*>(tex_pixels) + static_cast<size_t>(y) * tex_pitch);
            for (u16 x = 0; x < Config::WIDTH; ++x) dst[x] = lut[src[x]];
        }
        SDL_UnlockTexture(t);

        int ww=0, hh=0;
        SDL_GetRendererOutputSize(r,&ww,&hh);
        // Fit the fixed 16:9 render surface into the actual drawable area without
        // stretching. This keeps the reticle and every projected pixel centered
        // and preserves the renderer's aspect ratio under resize/fullscreen.
        const double src_aspect = double(Config::WIDTH) / double(Config::HEIGHT);
        const double dst_aspect = (hh > 0) ? double(ww) / double(hh) : src_aspect;
        SDL_Rect d{};
        if (dst_aspect > src_aspect) {
            d.h = hh;
            d.w = static_cast<int>(hh * src_aspect + 0.5);
            d.x = (ww - d.w) / 2;
            d.y = 0;
        } else {
            d.w = ww;
            d.h = static_cast<int>(ww / src_aspect + 0.5);
            d.x = 0;
            d.y = (hh - d.h) / 2;
        }
        SDL_RenderSetViewport(r, &d);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        SDL_RenderCopy(r,t,nullptr,nullptr);
        SDL_RenderPresent(r);
        SDL_RenderSetViewport(r, nullptr);
        ++frames; uint32_t sn=SDL_GetTicks(); if(sn-stat>=1000){ double fps=frames/((sn-stat)/1000.0); const auto& pl = game.player();
            const auto reserved_kib = MemoryBudget::TOTAL_RESERVED / 1024.0;
            const auto free_kib = (MemoryBudget::SRAM_BYTES - MemoryBudget::TOTAL_RESERVED) / 1024.0;
            std::cerr<<"\r[STATS] FPS="<<std::fixed<<std::setprecision(1)<<fps
                     <<" | SIM="<<sim_ticks<<" | POS=("<<(pl.pos.x.raw/65536.0)
                     <<","<<(pl.pos.y.raw/65536.0)<<") | ANG="<<pl.angle_turn
                     <<" | INPUT="<<(in.up?'W':(in.down?'S':(in.left?'L':(in.right?'R':(in.strafe_left?'A':(in.strafe_right?'D':(in.mouse_x!=0?'M':'-')))))))
                     <<" | RESV="<<reserved_kib<<" KiB"
                     <<" | FB8="<<(MemoryBudget::FRAMEBUFFER8/1024.0)<<" KiB x"<<MemoryBudget::DISPLAY_FRAME_BUFFERS
                     <<" | FREE="<<free_kib<<" KiB"
                     <<" | HOST-RSS="<<linux_rss_kib()<<" KiB"
                     <<" | HOST-HWM="<<linux_hwm_kib()<<" KiB      "<<std::flush; frames=0;stat=sn; }
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    std::cerr<<"\n"; SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit(); return 0;
}
