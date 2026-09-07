#include "api.h"
#include "assets.h"
#include "l3d_math.h"
#include "test_common.h"
#include <cstdint>
#include <vector>
#include <iostream>
#include <cstdlib>
int main(){
    std::vector<std::uint8_t> mem(l3d_engine_size()+l3d_engine_align());
    void* p=mem.data();
    std::uintptr_t a=reinterpret_cast<std::uintptr_t>(p), al=l3d_engine_align();
    p=reinterpret_cast<void*>((a+al-1)&~(al-1));
    L3D_REQUIRE(l3d_engine_init(p, l3d_engine_size()));
    auto pack=l3d::builtin_assets();
    L3D_REQUIRE(pack.valid());
    L3D_REQUIRE(l3d_engine_set_assets(p, pack.data, pack.size));
    std::vector<std::uint8_t> fb(l3d_framebuffer_bytes());
    l3d_framebuffer8 out{fb.data(),l3d_width(),l3d_height(),l3d_width()};
    l3d_input in{};
    for(int i=0;i<10;i++){ in.up=1; l3d_engine_update_ms(p,&in,16); L3D_REQUIRE(l3d_engine_render(p,&out)); }

    auto s0=l3d::TrigLut::sin16(0);
    auto c0=l3d::TrigLut::cos16(0);
    auto s90=l3d::TrigLut::sin16(0x4000);
    L3D_REQUIRE(std::abs(s0.raw) <= 2);
    L3D_REQUIRE(c0.raw > 65000);
    L3D_REQUIRE(s90.raw > 65000);
    L3D_REQUIRE(l3d_reserved_sram_bytes()<520u*1024u);
    L3D_REQUIRE(l3d_reserved_sram_bytes() > 300u*1024u);
    std::cout << "ABI smoke OK\n";
    std::cout << "engine=" << l3d_engine_size() << " bytes\n";
    std::cout << "framebuffer=" << l3d_framebuffer_bytes() << " bytes\n";
    std::cout << "reserved=" << l3d_reserved_sram_bytes() << " bytes\n";
}
