#!/usr/bin/env python3
"""Embed map.txt + raw textures as an L3DP v2 asset pack."""
from pathlib import Path
import sys

def fail(msg):
    print(f"embed_assets.py: error: {msg}", file=sys.stderr); return 2

def main():
    if len(sys.argv)!=4: print("usage: embed_assets.py MAP_TXT TEXTURES_RAW OUTPUT_CPP", file=sys.stderr); return 2
    map_path, tex_path, out_path=map(Path, sys.argv[1:])
    rows=[]
    for raw in map_path.read_text().splitlines():
        line=''.join(raw.split())
        if not line: continue
        if any(c.lower() not in '0123456789abcdef' for c in line): return fail('invalid map cell')
        rows.append([int(c,16) for c in line])
    if not rows or len({len(r) for r in rows})!=1: return fail('map must be non-empty and rectangular')
    h,w=len(rows),len(rows[0])
    if w>65535 or h>65535: return fail('map dimensions must fit u16')
    flat=[v for row in rows for v in row]; packed=bytearray((w*h+1)//2)
    for i,v in enumerate(flat): packed[i//2] |= (v<<4) if (i&1) else v
    tex=tex_path.read_bytes()
    if not tex or len(tex)%4096: return fail('texture payload must be non-empty multiple of 4096')
    count=len(tex)//4096
    if count>255: return fail('texture count >255')
    # v2: magic, version, u16 W/H, u16 texCount, u32 mapBytes, u32 texBytes, u16 reserved
    header=bytearray(20); header[0:4]=b'L3DP'; header[4]=2
    header[5:7]=int(w).to_bytes(2,'little'); header[7:9]=int(h).to_bytes(2,'little'); header[9:11]=int(count).to_bytes(2,'little')
    header[11:15]=int(len(packed)).to_bytes(4,'little'); header[15:19]=int(len(tex)).to_bytes(4,'little')
    blob=bytes(header)+bytes(packed)+tex
    lines=['    '+', '.join(f'0x{b:02X}' for b in blob[i:i+16])+',' for i in range(0,len(blob),16)]
    out_path.write_text(f'#include "assets.h"\n\nnamespace l3d {{\nnamespace {{ constexpr unsigned char GENERATED_ASSETS[] = {{\n'+'\n'.join(lines)+f'\n}}; }}\n\nAssetPackView generated_assets() {{ static_assert(sizeof(GENERATED_ASSETS)=={len(blob)}u,"generated asset size mismatch"); return {{GENERATED_ASSETS,sizeof(GENERATED_ASSETS)}}; }}\n}} // namespace l3d\n')
    print(f'generated {out_path}: {w}x{h}, {count} texture(s), {len(blob)} bytes'); return 0
if __name__=='__main__': raise SystemExit(main())
