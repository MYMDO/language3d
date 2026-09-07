#pragma once
#include "core.h"

namespace l3d {

// Small deterministic lookup-based trig helpers. The game/renderer can use these
// on MCUs without requiring an FPU. Angle is a signed 16-bit turn:
// 0 = 0°, 0x4000 = 90°, 0x8000 = 180°.
struct TrigLut {
    static Fx sin16(u16 turn);
    static Fx cos16(u16 turn);
};

inline u16 radians_to_turns(Fx radians) {
    // π radians == 0x8000 turns. This conversion is only used at platform/UI
    // boundaries; core simulation should keep angles in turn units.
    constexpr i32 PI_RAW = 205887; // pi * 65536
    return static_cast<u16>((static_cast<i64>(radians.raw) * 32768ll) / PI_RAW);
}

inline Fx turn16_to_fx(u16 turn) {
    return Fx::from_raw(static_cast<i32>((static_cast<u32>(turn) * 411775u) >> 16));
}

}
