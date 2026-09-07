#include "l3d_math.h"
#include <array>

namespace l3d {
namespace {
// 256-sample quarter-wave signed LUT, generated at runtime once in a tiny
// static object. This can be replaced by a constexpr table in a size-tuned
// MCU build; keeping the interface fixed is the important portability point.
std::array<i32,257> make_quarter(){
    std::array<i32,257> t{};
    // Piecewise-linear approximation of sin(x) over [0,pi/2] using a 5th-order
    // minimax-ish polynomial. No libm dependency in the renderer path.
    for(int i=0;i<=256;++i){
        const i64 x = i64(i) * 102943 / 256; // ~= pi/2 in 16.16
        const i64 x2=(x*x)>>16, x3=(x2*x)>>16, x5=(x3*x2)>>16;
        i64 y=x - x3/6 + x5/120;
        if (y < 0) y = 0;
        if (y > 65536) y = 65536;
        t[i]=i32(y);
    }
    return t;
}
const auto LUT = make_quarter();
inline i32 quarter(u16 phase){
    const u32 p = phase > 0x4000u ? 0x4000u : phase;
    const u32 idx=(p*256u)>>14;
    const u32 frac=(p*256u)&0x3FFFu;
    const i32 a=LUT[idx], b=LUT[std::min<u32>(256u,idx+1)];
    return a + i32((i64(b-a)*frac)>>14);
}
}
Fx TrigLut::sin16(u16 turn){
    const u16 phase=turn&0x3FFFu;
    const int quad=(turn>>14)&3;
    const u16 q=(quad==1||quad==3) ? u16(0x4000u-phase) : phase;
    i32 v=quarter(q);
    if(quad>=2) v=-v;
    return Fx::from_raw(v);
}
Fx TrigLut::cos16(u16 turn){ return sin16(static_cast<u16>(turn+0x4000u)); }
}
