#include "renderer.h"
#include "assets.h"
#include "test_common.h"
#include <vector>
#include <iostream>
int main(){
    std::vector<l3d::u8> pixels(l3d::MemoryBudget::FRAMEBUFFER8, 0);
    std::vector<l3d::u16> depth(l3d::Config::WIDTH, 0);
    l3d::Framebuffer8 fb{pixels.data(), l3d::Config::WIDTH, l3d::Config::HEIGHT, l3d::Config::WIDTH, depth.data()};
    l3d::SoftwareRaycaster r; r.set_assets(l3d::builtin_assets());
    l3d::Player p{}; l3d::NPC n{}; n.pos.x=l3d::Fx::from_raw(655360); n.pos.y=l3d::Fx::from_raw(294912);
    r.begin(fb); r.draw_world(fb,p,n); r.draw_hud(fb,"OBJ","STATUS"); r.end(fb);
    size_t nonzero=0, hit=0; for(auto v:pixels) nonzero += v!=0; for(auto d:depth) hit += d!=0xFFFFu;
    L3D_REQUIRE(nonzero > l3d::MemoryBudget::FRAMEBUFFER8/2);
    L3D_REQUIRE(hit > 0);
    std::cout << "Renderer pipeline test OK\n";
    std::cout << "nonzero=" << nonzero << " wall_columns=" << hit << "\n";
}
