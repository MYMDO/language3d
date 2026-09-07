#pragma once
#include "core.h"

namespace l3d {

// Small POD capability contract. Platform backends report what they can do;
// engine policies can choose an appropriate render path without runtime
// polymorphism or heavyweight scene/driver objects.
struct RendererCapabilities {
    bool indexed8{true};
    bool truecolor{false};
    bool sprites{true};
    bool textured_walls{true};
    bool textured_floor{false};
    bool dynamic_lights{false};
    bool shaders{false};
    bool gpu_acceleration{false};
    u16 max_width{Config::WIDTH};
    u16 max_height{Config::HEIGHT};
};

constexpr RendererCapabilities software_capabilities() {
    return RendererCapabilities{};
}

} // namespace l3d
