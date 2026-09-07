v0.32.1 — RP2040 ARM type-portability fix

Fixed GNU Arm Embedded C++17 build errors caused by int32_t being typedef'd as long on this target, which conflicted with untyped integer literals in std::max calls. The renderer now uses explicit i32 template arguments and checked casts at the affected call sites. Also fixed the RP2040 telemetry printf type warning for uint32_t frames.

No gameplay, renderer math, camera-alignment, framebuffer, ST7789 transport, or memory-budget behavior was intentionally changed.

v0.32.2: renamed engine/math.h -> engine/l3d_math.h to prevent Pico SDK <math.h> shadowing.
