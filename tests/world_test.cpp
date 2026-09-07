#include "world.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <vector>

static std::vector<std::uint8_t> make_pack() {
    constexpr int W=4, H=4;
    std::vector<std::uint8_t> cells(W*H,0);
    cells[0]=cells[1]=cells[2]=cells[3]=1;
    cells[12]=cells[13]=cells[14]=cells[15]=1;
    cells[4]=cells[8]=1; cells[7]=cells[11]=1;
    cells[5]=15; // door at (1,1)
    std::vector<std::uint8_t> out(16 + (W*H+1)/2 + 4096, 0);
    out[0]='L'; out[1]='3'; out[2]='D'; out[3]='P'; out[4]=1;
    out[5]=W; out[6]=H; out[7]=1; out[8]=0;
    const std::uint16_t mapBytes=(W*H+1)/2, texBytes=4096;
    out[10]=mapBytes & 255; out[11]=mapBytes >> 8;
    out[12]=texBytes & 255; out[13]=texBytes >> 8;
    for (std::size_t i=0;i<cells.size();++i) {
        if (i&1) out[16+i/2] |= static_cast<std::uint8_t>(cells[i]<<4);
        else out[16+i/2] |= cells[i];
    }
    for (std::size_t i=16+mapBytes;i<out.size();++i) out[i]=90;
    return out;
}

int main() {
    const auto bytes=make_pack();
    l3d::AssetPackView pack{bytes.data(), bytes.size()};
    L3D_REQUIRE(pack.valid());
    l3d::DoorSystem<2> doors;
    doors.init(pack);
    L3D_REQUIRE(doors.size()==1);
    L3D_REQUIRE(doors.blocks(1,1,pack));
    l3d::Vec2 p{l3d::Fx::from_raw((1<<16)),l3d::Fx::from_raw((1<<16))};
    L3D_REQUIRE(doors.toggle_near(p,1));
    for(int i=0;i<500;++i) doors.update(10);
    L3D_REQUIRE(doors.is_fully_open(1,1));
    L3D_REQUIRE(!doors.blocks(1,1,pack));
    std::printf("World/door test OK doors=%zu open=%u\n",doors.size(),unsigned(doors.data()[0].open));
    return 0;
}
