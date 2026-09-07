#include "assets.h"
#include <algorithm>

namespace l3d {
namespace {
constexpr size_t HDR = 16;
constexpr size_t MAP_X = 24, MAP_Y = 24, MAP_BYTES = (MAP_X * MAP_Y + 1) / 2;
constexpr size_t TEX_SIZE = 2 * 64 * 64;
constexpr u8 MAP[] = {
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x01,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x11,
0x11,
0x01,
0x10,
0x11,
0x11,
0x00,
0x11,
0x11,
0x11,
0x11,
0x11,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x10,
0x11,
0x11,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x10,
0x00,
0x00,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x10,
0x00,
0x00,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x10,
0x11,
0x11,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x11,
0x11,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x11,
0x11,
0x11,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x01,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x10,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11,
0x11
};
constexpr size_t HEADER_AND_MAP = HDR + MAP_BYTES;
// A procedural texture is emitted into a static buffer at first use; this is only the
// built-in fallback. External packs remain fully file-backed and allocation-free.
u8 BUILTIN[HEADER_AND_MAP + TEX_SIZE]{};
bool initialized=false;

void init_builtin(){
    if(initialized) return;
    BUILTIN[0]='L'; BUILTIN[1]='3'; BUILTIN[2]='D'; BUILTIN[3]='P';
    BUILTIN[4]=1; BUILTIN[5]=24; BUILTIN[6]=24; BUILTIN[7]=2; BUILTIN[8]=0;
    BUILTIN[9]=0; BUILTIN[10]=static_cast<u8>(MAP_BYTES & 255); BUILTIN[11]=static_cast<u8>(MAP_BYTES >> 8);
    BUILTIN[12]=static_cast<u8>(TEX_SIZE & 255); BUILTIN[13]=static_cast<u8>(TEX_SIZE >> 8); BUILTIN[14]=0; BUILTIN[15]=0;
    for(size_t i=0;i<MAP_BYTES;++i) BUILTIN[HDR+i]=MAP[i];
    u8* t=BUILTIN+HEADER_AND_MAP;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x){
        const int brick=((x/8)+(y/8))&1;
        const int mortar=(x%8==0)||(y%8==0);
        t[y*64+x]=static_cast<u8>(mortar ? 52 : (brick ? 102 : 88));
    }
    u8* s=t+64*64;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x){
        const int cx=x-32, cy=y-32;
        const int r2=cx*cx+cy*cy;
        const bool body=r2<700;
        const bool eye=(cy<-8 && ((cx>-16 && cx<-4) || (cx>4 && cx<16)));
        s[y*64+x]=static_cast<u8>(body ? (eye ? 245 : 170) : 0);
    }
    initialized=true;
}
inline u16 rd16(const u8* p){return static_cast<u16>(p[0] | (u16(p[1])<<8));}
inline u32 rd32(const u8* p){return static_cast<u32>(p[0] | (u32(p[1])<<8) | (u32(p[2])<<16) | (u32(p[3])<<24));}
inline bool is_v2(const u8* d, size_t n){ return d && n>=5 && d[0]=='L'&&d[1]=='3'&&d[2]=='D'&&d[3]=='P'&&d[4]==2; }
inline size_t header_size(const u8* d, size_t n){ return is_v2(d,n) ? 20u : 16u; }
inline size_t map_bytes_at(const u8* d, size_t n){ return is_v2(d,n) ? size_t(rd32(d+11)) : size_t(rd16(d+10)); }
inline size_t tex_bytes_at(const u8* d, size_t n){ return is_v2(d,n) ? size_t(rd32(d+15)) : size_t(rd16(d+12)); }
inline u16 map_w_at(const u8* d, size_t n){ return is_v2(d,n) ? rd16(d+5) : (n>=6 ? d[5] : 0); }
inline u16 map_h_at(const u8* d, size_t n){ return is_v2(d,n) ? rd16(d+7) : (n>=7 ? d[6] : 0); }
inline u8 tex_count_at(const u8* d, size_t n){ return is_v2(d,n) ? static_cast<u8>(rd16(d+9)) : (n>=8 ? d[7] : 0); }
}

bool AssetPackView::valid() const { return asset_pack_validate(data,size); }
const u8* AssetPackView::map4() const { return data ? data + header_size(data,size) : nullptr; }
u16 AssetPackView::map_width() const { return data ? map_w_at(data,size) : 0; }
u16 AssetPackView::map_height() const { return data ? map_h_at(data,size) : 0; }
u8 AssetPackView::texture_count() const { return data ? tex_count_at(data,size) : 0; }
const u8* AssetPackView::texture8(u8 index) const {
    if(!valid() || index>=texture_count()) return nullptr;
    return data + header_size(data,size) + map_bytes_at(data,size) + size_t(index) * 64u * 64u;
}

bool asset_pack_validate(const u8* data, size_t size){
    if(!data || size<16 || data[0]!='L'||data[1]!='3'||data[2]!='D'||data[3]!='P') return false;
    const bool v1=data[4]==1, v2=data[4]==2;
    if(!v1 && !v2) return false;
    const size_t hs=v2?20u:16u;
    if(size<hs) return false;
    const size_t w=map_w_at(data,size), h=map_h_at(data,size), tc=tex_count_at(data,size);
    const size_t mapBytes=map_bytes_at(data,size), texBytes=tex_bytes_at(data,size);
    return w>0 && h>0 && tc>0 && mapBytes>=((w*h+1)/2) && texBytes>=tc*64u*64u && hs+mapBytes+texBytes<=size;
}

u8 packed_map_cell4(const AssetPackView& p, i32 x, i32 y){
    if(!p.valid() || x<0 || y<0 || x>=p.map_width() || y>=p.map_height()) return 1;
    const size_t idx=size_t(y)*p.map_width()+size_t(x);
    const u8 byte=p.map4()[idx>>1];
    return (idx&1) ? (byte>>4) : (byte&0x0F);
}

u8 texture_sample64(const AssetPackView& p, u8 texture, u8 u, u8 v){
    const u8* t=p.texture8(texture); if(!t) return 0;
    return t[size_t(v&63u)*64u+(u&63u)];
}

u8 material_texture(const AssetPackView& p, u8 material){
    if(!p.valid() || p.texture_count()==0 || material==0) return 0;
    // L3DP v1 has no material table yet. Keep all opaque world materials on
    // texture slot 0; sprite texture slots are selected explicitly by entities.
    return 0;
}

AssetPackView builtin_assets(){ init_builtin(); return {BUILTIN,sizeof(BUILTIN)}; }
}
