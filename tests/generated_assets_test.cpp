#include "assets.h"
#include "test_common.h"
#include <iostream>
int main(){
    const auto p = l3d::generated_assets();
    L3D_REQUIRE(p.valid());
    // Map dimensions follow assets/map.txt (the pack is generated at build
    // time by tools/embed_assets.py), so only structural invariants are
    // asserted here instead of a hardcoded size.
    L3D_REQUIRE(p.map_width() > 0 && p.map_height() > 0);
    L3D_REQUIRE(p.texture_count() == 2);
    L3D_REQUIRE(l3d::packed_map_cell4(p,4,4) == 0);
    L3D_REQUIRE(l3d::packed_map_cell4(p,18,18) == 0);
    L3D_REQUIRE(l3d::packed_map_cell4(p,0,0) == 1);
    std::cout << "generated assets OK: " << p.map_width() << 'x' << p.map_height()
              << " textures=" << unsigned(p.texture_count()) << " bytes=" << p.size << "\n";
}
