#include "../../engine/game.h"
#include "../../platform/api/l3d_platform.h"
#include "scenario.h"
#include "playtest/playtest_cli.h"
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
    // Playtest modes run before SDL init and never touch the platform layer.
    const int pt = playtest::playtest_cli(argc, argv);
    if (pt >= 0) return pt;
    if (!l3d_pf_init("Language 3D MVP — PC Linux")) {
        std::fprintf(stderr, "platform init failed\n");
        return 1;
    }
    // Large desktop buffers live on the heap, never on the thread stack. The engine
    // itself remains a caller-owned indexed8 framebuffer, preserving the portable ABI.
    std::vector<u8> pixels(MemoryBudget::FRAMEBUFFER8);
    Framebuffer8 fb{pixels.data(), Config::WIDTH, Config::HEIGHT, Config::WIDTH};
    SoftwareRaycaster renderer;
    Game game;
    const AssetPackView game_assets = generated_assets();
    if (!game_assets.valid()) {
        std::fprintf(stderr, "generated asset pack invalid; refusing to run with mismatched content\n");
        l3d_pf_shutdown();
        return 1;
    }
    game.set_assets(game_assets);    std::cerr << "[ASSET] map=" << game_assets.map_width() << "x" << game_assets.map_height()
              << " | textures=" << unsigned(game_assets.texture_count())
              << " | bytes=" << game_assets.size << "\n";
    game.reset();

    // Phase 14 vertical slice: scenario orchestration over the live game.
    // The maze game runs unchanged; the slice reads player state, mirrors
    // NPC sprites, consumes E/answers/F5/F9 when it handles them, and draws
    // its panel/HUD into the framebuffer before presentation.
    Scenario scenario{};
    const DialogueBank slice_dbank = content_dialogues();
    const QuestBank slice_qbank = content_quests();
    const ItemBank slice_ibank = content_items();
    const VocabularyBank slice_vbank = content_vocabulary();
    bool sliceOn = scenario.init(&slice_dbank, &slice_qbank, &slice_ibank,
                                 &slice_vbank);
    if (!sliceOn) std::fprintf(stderr, "scenario init failed; maze only\n");
    auto slice_solid = [](void* ctx, int32_t x, int32_t y) {
        return static_cast<Game*>(ctx)->solid(x, y);
    };

    SimulationClock sim_clock{};
    bool run=true; uint32_t last=l3d_pf_ticks_ms(),stat=last; uint64_t frames=0, sim_ticks=0;
    bool debug_camera=false;
    bool mouse_look=false;
    bool fullscreen=false;
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
    // Desktop input latch: held state is driven by key edge events, while the
    // platform key snapshot is retained as a recovery path for backends that
    // may drop an event during focus transitions. One-shot actions persist
    // until a simulation tick consumes them.
    struct InputLatch {
        bool up=false, down=false, left=false, right=false;
        bool strafe_left=false, strafe_right=false;
        bool interact=false, language=false, progress=false, help=false, escape=false;
        bool save=false, load=false;
        u8 answer=0;
    } latch;
    i16 latch_mouse_x = 0;

    while(run){
        uint32_t now=l3d_pf_ticks_ms(); uint32_t frame_ms=now-last; last=now;
        InputState in{};
        l3d_event ev{};
        while(l3d_pf_poll(&ev)){
            if(ev.type==L3D_EVENT_QUIT) { run=false; continue; }
            if(ev.type==L3D_EVENT_FOCUS_LOST){
                latch.up=latch.down=latch.left=latch.right=false;
                latch.strafe_left=latch.strafe_right=false;
                if(mouse_look){ l3d_pf_set_relative_mouse(0); mouse_look=false; }
            }
            if(ev.type==L3D_EVENT_MOUSE_BUTTON && ev.key==L3D_MOUSE_LEFT && ev.pressed){
                l3d_pf_set_relative_mouse(1);
                mouse_look=true;
                l3d_pf_show_cursor(0);
            }
            if(ev.type==L3D_EVENT_MOUSE_MOTION && mouse_look){
                const int rel = std::clamp(int(ev.dx), -2048, 2048);
                const i32 accum = i32(latch_mouse_x) + i32(rel);
                latch_mouse_x = static_cast<i16>(std::clamp(accum, -32768, 32767));
            }
            if(ev.type==L3D_EVENT_KEY_DOWN && !ev.repeat && ev.key==L3D_KEY_ESCAPE && mouse_look){
                l3d_pf_set_relative_mouse(0);
                mouse_look=false;
                l3d_pf_show_cursor(1);
                continue;
            }
            if(ev.type==L3D_EVENT_KEY_DOWN || ev.type==L3D_EVENT_KEY_UP){
                const bool down = (ev.type==L3D_EVENT_KEY_DOWN);
                switch(ev.key){
                    case L3D_KEY_W: latch.up=down; break;
                    case L3D_KEY_S: latch.down=down; break;
                    case L3D_KEY_A: latch.strafe_left=down; break;
                    case L3D_KEY_D: latch.strafe_right=down; break;
                    case L3D_KEY_LEFT: latch.left=down; break;
                    case L3D_KEY_RIGHT: latch.right=down; break;
                    case L3D_KEY_UP: latch.up=down; break;
                    case L3D_KEY_DOWN: latch.down=down; break;
                    case L3D_KEY_Q: latch.strafe_left=down; break;
                    case L3D_KEY_C: latch.strafe_right=down; break;
                    default: break;
                }
                if(down && !ev.repeat){
                    switch(ev.key){
                        case L3D_KEY_E: latch.interact=true; break;
                        case L3D_KEY_L: latch.language=true; break;
                        case L3D_KEY_P: latch.progress=true; break;
                        case L3D_KEY_H: latch.help=true; break;
                        case L3D_KEY_ESCAPE: latch.escape=true; break;
                        case L3D_KEY_1: latch.answer=1; break;
                        case L3D_KEY_2: latch.answer=2; break;
                        case L3D_KEY_3: latch.answer=3; break;
                        case L3D_KEY_F3: debug_camera = !debug_camera; break;
                        case L3D_KEY_F5: latch.save=true; break;
                        case L3D_KEY_F9: latch.load=true; break;
                        case L3D_KEY_F11:
                            fullscreen = !fullscreen;
                            l3d_pf_set_fullscreen(fullscreen ? 1 : 0);
                            break;
                        default: break;
                    }
                }
            }
        }

        in.up    = latch.up    || l3d_pf_key_down(L3D_KEY_W) || l3d_pf_key_down(L3D_KEY_UP);
        in.down  = latch.down  || l3d_pf_key_down(L3D_KEY_S) || l3d_pf_key_down(L3D_KEY_DOWN);
        in.left  = latch.left  || l3d_pf_key_down(L3D_KEY_LEFT);
        in.right = latch.right || l3d_pf_key_down(L3D_KEY_RIGHT);
        in.strafe_left  = latch.strafe_left  || l3d_pf_key_down(L3D_KEY_A) || l3d_pf_key_down(L3D_KEY_Q);
        in.strafe_right = latch.strafe_right || l3d_pf_key_down(L3D_KEY_D) || l3d_pf_key_down(L3D_KEY_C);
        in.interact_pressed = latch.interact;
        in.language_pressed = latch.language;
        in.progress_pressed = latch.progress;
        in.help_pressed = latch.help;
        in.escape_pressed = latch.escape;
        in.answer = latch.answer;
        in.mouse_x = mouse_look ? latch_mouse_x : 0;

        // Vertical slice wiring (no Game changes: read state, own sprites,
        // consume handled keys, draw panel before presentation).
        if (sliceOn) {
            Transform3 slicePlayer{};
            slicePlayer.pos =
                Vec3{game.player().pos.x, game.player().pos.y, Fx{}};
            slicePlayer.yaw = game.player().angle_turn;
            scenario.setPlayer(slicePlayer);
            scenario.tick(frame_ms, now, slice_solid, &game);
            game.sprites().clear();
            SpriteEntity sprA{};
            sprA.pos = scenario.annaPos();
            sprA.texture = 1;
            sprA.width_scale = 32;
            sprA.height_scale = 64;
            SpriteEntity sprC = sprA;
            sprC.pos = scenario.clerkPos();
            game.sprites().add(sprA);
            game.sprites().add(sprC);
            if (latch.interact && scenario.pressE(now)) latch.interact = false;
            if (latch.answer && scenario.pressAnswer(latch.answer))
                latch.answer = 0;
            if (latch.save) {
                scenario.saveGame("language3d.save");
                latch.save = false;
            }
            if (latch.load) {
                scenario.loadGame("language3d.save");
                latch.load = false;
            }
        }

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
        if (sliceOn) scenario.renderPanel(fb.pixels, fb.width, fb.height, fb.stride);

        // Presentation (upload + letterbox + flip) is owned by the backend.
        const auto& lut = renderer.palette().rgb565;
        if (!l3d_pf_present_indexed8(fb.pixels, fb.width, fb.height, fb.stride, lut.data())) {
            std::fprintf(stderr, "present failed\n");
            break;
        }
        ++frames; uint32_t sn=l3d_pf_ticks_ms(); if(sn-stat>=1000){ double fps=frames/((sn-stat)/1000.0); const auto& pl = game.player();
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
    l3d_pf_shutdown();
    std::cerr<<"\n"; return 0;
}
