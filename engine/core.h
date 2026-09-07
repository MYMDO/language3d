#pragma once
#include <cstdint>
#include <cstddef>

namespace l3d {

using i8 = int8_t; using u8 = uint8_t; using i16 = int16_t; using u16 = uint16_t;
using i32 = int32_t; using u32 = uint32_t; using i64 = int64_t; using u64 = uint64_t;

// 16.16 signed fixed point. Canonical simulation representation across targets.
struct Fx {
    i32 raw{0};
    static constexpr int SHIFT = 16;
    static constexpr i32 ONE = 1 << SHIFT;
    constexpr Fx() = default;
    explicit constexpr Fx(i32 integer) : raw(integer << SHIFT) {}
    static constexpr Fx from_raw(i32 v) { Fx f; f.raw = v; return f; }
    static constexpr Fx from_int(i32 v) { return Fx(v); }
    static Fx from_float(float v) { return from_raw(static_cast<i32>(v * static_cast<float>(ONE))); }
    constexpr i32 to_int() const { return raw >> SHIFT; }
    float to_float() const { return static_cast<float>(raw) / static_cast<float>(ONE); }
};

constexpr Fx operator+(Fx a, Fx b) { return Fx::from_raw(a.raw + b.raw); }
constexpr Fx operator-(Fx a, Fx b) { return Fx::from_raw(a.raw - b.raw); }
constexpr Fx operator-(Fx a) { return Fx::from_raw(-a.raw); }
inline Fx operator*(Fx a, Fx b) { return Fx::from_raw(static_cast<i32>((static_cast<int64_t>(a.raw) * b.raw) >> Fx::SHIFT)); }
inline Fx operator/(Fx a, Fx b) { return Fx::from_raw(static_cast<i32>((static_cast<int64_t>(a.raw) << Fx::SHIFT) / b.raw)); }
constexpr bool operator<(Fx a, Fx b) { return a.raw < b.raw; }
constexpr bool operator>(Fx a, Fx b) { return a.raw > b.raw; }
constexpr bool operator<=(Fx a, Fx b) { return a.raw <= b.raw; }
constexpr bool operator>=(Fx a, Fx b) { return a.raw >= b.raw; }
constexpr bool operator==(Fx a, Fx b) { return a.raw == b.raw; }
constexpr bool operator!=(Fx a, Fx b) { return a.raw != b.raw; }

struct Vec2 { Fx x{}, y{}; };
struct Player { Vec2 pos{Fx::from_raw(294912), Fx::from_raw(294912)}; u16 angle_turn{}; };
struct NPC { Vec2 pos{Fx::from_raw(1212416), Fx::from_raw(1212416)}; };
struct Progress { u8 prioritize{0}, deadline{0}, efficient{0}; };

enum class Mode : u8 { Explore, Dialogue, Quiz, Progress, Help };

struct InputState {
    bool up{false}, down{false}, left{false}, right{false};
    bool strafe_left{false}, strafe_right{false};
    // Mouse delta is optional and platform-owned; zero means no mouse input.
    i16 mouse_x{0};
    bool interact_pressed{false}, language_pressed{false};
    bool progress_pressed{false}, help_pressed{false}, escape_pressed{false};
    u8 answer{0};
};

struct ControlTuning {
    // World units / second, 16.16 fixed-point.
    static constexpr i32 MOVE_SPEED_RAW = 157286;   // 2.4
    static constexpr i32 STRAFE_SPEED_RAW = 144179; // 2.2
    // Turn units / second. 65536 units = 360 degrees.
    static constexpr u16 TURN_SPEED = 12000;
    static constexpr u16 MOUSE_TURN_PER_PIXEL = 64; // 0.35 deg/pixel; slower FPS-style mouse look
};

struct Config {
#if defined(L3D_PC_PROFILE)
    // Desktop profile: render natively at 1920×1080.
    // This is a native-resolution PC profile; embedded profiles remain independent.
    static constexpr u16 WIDTH = 1920;
    static constexpr u16 HEIGHT = 1080;
    static constexpr u16 FPS = 60;
    static constexpr u32 MAX_CATCHUP_TICKS = 4;
#else
    static constexpr u16 WIDTH = 240;
    static constexpr u16 HEIGHT = 240;
    static constexpr u16 FPS = 60;
    static constexpr u32 MAX_CATCHUP_TICKS = 4;
#endif
};

// Deterministic 60 Hz simulation-rate clock. An exact 60-tick cadence is
// represented as [16,17,17] ms repeated 20 times: 20*16 + 40*17 = 1000 ms.
// Platform frame cadence remains independent, while catch-up is bounded after stalls.
struct SimulationClock {
    u32 accumulator_ms{0};
    u8 tick_index{0};

    static constexpr u8 tick_duration_ms(u8 index) {
        return (index % 3u) == 0u ? u8(16u) : u8(17u);
    }

    u8 next_tick_ms() const { return tick_duration_ms(tick_index); }

    template <typename StepFn>
    void advance(u32 frame_dt_ms, StepFn&& step) {
        if (frame_dt_ms > 250u) frame_dt_ms = 250u;
        accumulator_ms += frame_dt_ms;
        u32 ticks = 0;
        while (accumulator_ms >= next_tick_ms() && ticks < Config::MAX_CATCHUP_TICKS) {
            const u8 dt = next_tick_ms();
            accumulator_ms -= dt;
            step(dt);
            ++ticks;
            tick_index = static_cast<u8>((tick_index + 1u) % 60u);
        }
        if (ticks == Config::MAX_CATCHUP_TICKS && accumulator_ms >= next_tick_ms()) {
            accumulator_ms = next_tick_ms() - 1u;
        }
    }
};

// Deliberate upper bounds for deterministic MCU allocation.
struct MemoryBudget {
#if defined(L3D_PC_PROFILE)
    static constexpr size_t SRAM_BYTES = 16ull * 1024ull * 1024ull * 1024ull;
#elif defined(L3D_RP2040_PROFILE)
    static constexpr size_t SRAM_BYTES = 264u * 1024u;
#else
    static constexpr size_t SRAM_BYTES = 520u * 1024u;
#endif
    static constexpr size_t FRAMEBUFFER8 = static_cast<size_t>(Config::WIDTH) * Config::HEIGHT; // 76,800
#if defined(L3D_PC_PROFILE)
    static constexpr size_t DISPLAY_FRAME_BUFFERS = 1;
#elif defined(L3D_RP2040_PROFILE)
    static constexpr size_t DISPLAY_FRAME_BUFFERS = 1;
#else
    static constexpr size_t DISPLAY_FRAME_BUFFERS = 2;
#endif
    static constexpr size_t DISPLAY_FRAMEBUFFER8 = FRAMEBUFFER8 * DISPLAY_FRAME_BUFFERS;
    static constexpr size_t DEPTH16 = static_cast<size_t>(Config::WIDTH) * sizeof(u16);          // 640; one depth value per screen column
    static constexpr size_t ENGINE_STATE = 8u * 1024u;
    static constexpr size_t ENTITY_POOL = 8u * 1024u;
    static constexpr size_t RESOURCE_WORK = 64u * 1024u;
    static constexpr size_t AUDIO = 64u * 1024u;
    static constexpr size_t COMMANDS = 16u * 1024u;
    static constexpr size_t RESOURCE_CACHE = 8u * 16u;
    static constexpr size_t STACK_RESERVE = 32u * 1024u;
    static constexpr size_t TOTAL_RESERVED = DISPLAY_FRAMEBUFFER8 + DEPTH16 + ENGINE_STATE + ENTITY_POOL + RESOURCE_WORK + AUDIO + COMMANDS + RESOURCE_CACHE + STACK_RESERVE;
    static_assert(DISPLAY_FRAME_BUFFERS >= 1 && DISPLAY_FRAME_BUFFERS <= 3, "unsupported display buffer count");
    static_assert(TOTAL_RESERVED < SRAM_BYTES, "RP2350B SRAM budget exceeded");
};

}
