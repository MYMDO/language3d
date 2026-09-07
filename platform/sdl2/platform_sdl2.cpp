// SDL2 backend for the Language3D thin Platform API.
//
// Owns the window/renderer/streaming-texture triple and the indexed8 ->
// RGB565 upload + aspect-preserving letterbox that used to live inline in
// the desktop main loop. Behavior is preserved 1:1; only the address of the
// code changed (backend instead of game loop).
#include "l3d_platform.h"

#include "../../engine/core.h" // Config::WIDTH/HEIGHT render-target size

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdint>

namespace {

struct SdlBackend {
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    SDL_Texture* texture{nullptr};
    bool ready{false};
};

SdlBackend g_sdl{};

l3d_key_t map_scancode(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_W: return L3D_KEY_W;
        case SDL_SCANCODE_A: return L3D_KEY_A;
        case SDL_SCANCODE_S: return L3D_KEY_S;
        case SDL_SCANCODE_D: return L3D_KEY_D;
        case SDL_SCANCODE_Q: return L3D_KEY_Q;
        case SDL_SCANCODE_C: return L3D_KEY_C;
        case SDL_SCANCODE_E: return L3D_KEY_E;
        case SDL_SCANCODE_L: return L3D_KEY_L;
        case SDL_SCANCODE_P: return L3D_KEY_P;
        case SDL_SCANCODE_H: return L3D_KEY_H;
        case SDL_SCANCODE_LEFT: return L3D_KEY_LEFT;
        case SDL_SCANCODE_RIGHT: return L3D_KEY_RIGHT;
        case SDL_SCANCODE_UP: return L3D_KEY_UP;
        case SDL_SCANCODE_DOWN: return L3D_KEY_DOWN;
        case SDL_SCANCODE_ESCAPE: return L3D_KEY_ESCAPE;
        case SDL_SCANCODE_1: return L3D_KEY_1;
        case SDL_SCANCODE_2: return L3D_KEY_2;
        case SDL_SCANCODE_3: return L3D_KEY_3;
        case SDL_SCANCODE_F3: return L3D_KEY_F3;
        case SDL_SCANCODE_F11: return L3D_KEY_F11;
        default: break;
    }
    return L3D_KEY_UNKNOWN;
}

SDL_Scancode unmap_key(uint8_t key) {
    switch (key) {
        case L3D_KEY_W: return SDL_SCANCODE_W;
        case L3D_KEY_A: return SDL_SCANCODE_A;
        case L3D_KEY_S: return SDL_SCANCODE_S;
        case L3D_KEY_D: return SDL_SCANCODE_D;
        case L3D_KEY_Q: return SDL_SCANCODE_Q;
        case L3D_KEY_C: return SDL_SCANCODE_C;
        case L3D_KEY_E: return SDL_SCANCODE_E;
        case L3D_KEY_L: return SDL_SCANCODE_L;
        case L3D_KEY_P: return SDL_SCANCODE_P;
        case L3D_KEY_H: return SDL_SCANCODE_H;
        case L3D_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case L3D_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case L3D_KEY_UP: return SDL_SCANCODE_UP;
        case L3D_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case L3D_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
        case L3D_KEY_1: return SDL_SCANCODE_1;
        case L3D_KEY_2: return SDL_SCANCODE_2;
        case L3D_KEY_3: return SDL_SCANCODE_3;
        case L3D_KEY_F3: return SDL_SCANCODE_F3;
        case L3D_KEY_F11: return SDL_SCANCODE_F11;
        default: break;
    }
    return SDL_SCANCODE_UNKNOWN;
}

} // namespace

extern "C" int l3d_pf_init(const char* title) {
    if (g_sdl.ready) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 0;
    const int w = int(l3d::Config::WIDTH);
    const int h = int(l3d::Config::HEIGHT);
    g_sdl.window = SDL_CreateWindow(title ? title : "Language3D",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_sdl.window) { SDL_Quit(); return 0; }
    g_sdl.renderer = SDL_CreateRenderer(g_sdl.window, -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_sdl.renderer)
        g_sdl.renderer = SDL_CreateRenderer(g_sdl.window, -1,
                                            SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_SOFTWARE);
    if (!g_sdl.renderer) {
        SDL_DestroyWindow(g_sdl.window);
        g_sdl.window = nullptr;
        SDL_Quit();
        return 0;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    g_sdl.texture = SDL_CreateTexture(g_sdl.renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!g_sdl.texture) {
        SDL_DestroyRenderer(g_sdl.renderer);
        SDL_DestroyWindow(g_sdl.window);
        g_sdl.renderer = nullptr;
        g_sdl.window = nullptr;
        SDL_Quit();
        return 0;
    }
    g_sdl.ready = true;
    return 1;
}

extern "C" void l3d_pf_shutdown(void) {
    if (!g_sdl.ready) return;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_DestroyTexture(g_sdl.texture);
    SDL_DestroyRenderer(g_sdl.renderer);
    SDL_DestroyWindow(g_sdl.window);
    g_sdl = SdlBackend{};
    SDL_Quit();
}

extern "C" uint16_t l3d_pf_display_width(void) { return l3d::Config::WIDTH; }
extern "C" uint16_t l3d_pf_display_height(void) { return l3d::Config::HEIGHT; }

extern "C" void l3d_pf_display_size(uint16_t* w, uint16_t* h) {
    int ww = int(l3d::Config::WIDTH), hh = int(l3d::Config::HEIGHT);
    if (g_sdl.ready) SDL_GetRendererOutputSize(g_sdl.renderer, &ww, &hh);
    if (w) *w = uint16_t(ww < 0 ? 0 : ww);
    if (h) *h = uint16_t(hh < 0 ? 0 : hh);
}

extern "C" void l3d_pf_set_fullscreen(int on) {
    if (!g_sdl.ready) return;
    SDL_SetWindowFullscreen(g_sdl.window,
                            on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

extern "C" uint32_t l3d_pf_ticks_ms(void) { return uint32_t(SDL_GetTicks()); }

extern "C" int l3d_pf_poll(l3d_event_t* out) {
    if (!out) return 0;
    SDL_Event e;
    // Skip unmapped keys so the queue only carries game-meaningful input.
    for (;;) {
        if (!SDL_PollEvent(&e)) return 0;
        l3d_event_t ev{};
        switch (e.type) {
            case SDL_QUIT:
                ev.type = L3D_EVENT_QUIT;
                *out = ev;
                return 1;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    ev.type = L3D_EVENT_FOCUS_LOST;
                    *out = ev;
                    return 1;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ev.type = L3D_EVENT_MOUSE_BUTTON;
                    ev.key = L3D_MOUSE_LEFT;
                    ev.pressed = 1;
                    *out = ev;
                    return 1;
                }
                break;
            case SDL_MOUSEMOTION:
                ev.type = L3D_EVENT_MOUSE_MOTION;
                ev.dx = int16_t(std::clamp(e.motion.xrel, -2048, 2048));
                ev.dy = int16_t(std::clamp(e.motion.yrel, -2048, 2048));
                *out = ev;
                return 1;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                const l3d_key_t k = map_scancode(e.key.keysym.scancode);
                if (k == L3D_KEY_UNKNOWN) break;
                ev.type = (e.type == SDL_KEYDOWN) ? L3D_EVENT_KEY_DOWN : L3D_EVENT_KEY_UP;
                ev.key = uint8_t(k);
                ev.pressed = (e.type == SDL_KEYDOWN) ? uint8_t(1) : uint8_t(0);
                ev.repeat = (e.key.repeat != 0) ? uint8_t(1) : uint8_t(0);
                *out = ev;
                return 1;
            }
            default:
                break;
        }
    }
}

extern "C" int l3d_pf_key_down(uint8_t key) {
    const SDL_Scancode sc = unmap_key(key);
    if (sc == SDL_SCANCODE_UNKNOWN) return 0;
    SDL_PumpEvents();
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    return ks[sc] ? 1 : 0;
}

extern "C" void l3d_pf_set_relative_mouse(int on) {
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
}

extern "C" void l3d_pf_show_cursor(int show) {
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

extern "C" int l3d_pf_present_indexed8(const uint8_t* pixels, uint16_t width,
                                       uint16_t height, uint32_t stride,
                                       const uint16_t* palette_rgb565) {
    if (!g_sdl.ready || !pixels || !palette_rgb565) return 0;
    if (width != l3d::Config::WIDTH || height != l3d::Config::HEIGHT) return 0;
    if (stride < width) return 0;

    // Indexed8 pixels directly into the streaming RGB565 texture: one LUT
    // lookup per pixel, no ARGB8888 staging buffer.
    void* tex_pixels = nullptr;
    int tex_pitch = 0;
    if (SDL_LockTexture(g_sdl.texture, nullptr, &tex_pixels, &tex_pitch) != 0)
        return 0;
    for (uint16_t y = 0; y < height; ++y) {
        const uint8_t* src = pixels + size_t(y) * stride;
        uint16_t* dst = reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(tex_pixels) + size_t(y) * size_t(tex_pitch));
        for (uint16_t x = 0; x < width; ++x) dst[x] = palette_rgb565[src[x]];
    }
    SDL_UnlockTexture(g_sdl.texture);

    // Fit the fixed-aspect render surface into the actual drawable area.
    uint16_t ww = 0, hh = 0;
    l3d_pf_display_size(&ww, &hh);
    const double src_aspect = double(width) / double(height);
    const double dst_aspect = (hh > 0) ? double(ww) / double(hh) : src_aspect;
    SDL_Rect d{};
    if (dst_aspect > src_aspect) {
        d.h = hh;
        d.w = int(hh * src_aspect + 0.5);
        d.x = (ww - d.w) / 2;
        d.y = 0;
    } else {
        d.w = ww;
        d.h = int(ww / src_aspect + 0.5);
        d.x = 0;
        d.y = (hh - d.h) / 2;
    }
    SDL_RenderSetViewport(g_sdl.renderer, &d);
    SDL_SetRenderDrawColor(g_sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_sdl.renderer);
    SDL_RenderCopy(g_sdl.renderer, g_sdl.texture, nullptr, nullptr);
    SDL_RenderPresent(g_sdl.renderer);
    SDL_RenderSetViewport(g_sdl.renderer, nullptr);
    return 1;
}
