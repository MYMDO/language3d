#include "assets.h"
#include "test_common.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
int main(){
    auto p=l3d::builtin_assets();
    L3D_REQUIRE(p.valid());
    L3D_REQUIRE(p.map_width()==24 && p.map_height()==24);
    L3D_REQUIRE(p.texture_count()==2);
    L3D_REQUIRE(l3d::packed_map_cell4(p,0,0)==1);
    L3D_REQUIRE(l3d::packed_map_cell4(p,1,1)==0);
    L3D_REQUIRE(l3d::texture_sample64(p,0,1,1)!=0);
    L3D_REQUIRE(l3d::material_texture(p,1)==0);
    L3D_REQUIRE(l3d::material_texture(p,15)==0);
    L3D_REQUIRE(l3d::texture_sample64(p,1,32,32)!=0);
    std::cout << "Asset pack test OK\\n";
    std::cout << "map=" << p.map_width() << "x" << p.map_height() << " textures=" << unsigned(p.texture_count()) << "\\n";
}
