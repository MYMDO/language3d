#!/usr/bin/env python3
"""Pack map.txt + indexed 64x64 textures into L3DP v2.

Usage:
  pack_assets.py map.txt textures.raw out.l3dp
"""
from pathlib import Path
import struct, sys

def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    map_path, tex_path, out_path = (Path(x) for x in sys.argv[1:])
    rows=[]
    for raw in map_path.read_text().splitlines():
        line=''.join(raw.split())
        if not line:
            continue
        if any(ch.lower() not in '0123456789abcdef' for ch in line):
            raise ValueError('invalid map cell')
        rows.append([int(ch,16) for ch in line])
    if not rows or len({len(r) for r in rows}) != 1:
        raise ValueError('map must be non-empty and rectangular')
    h,w=len(rows),len(rows[0])
    if w>65535 or h>65535:
        raise ValueError('map dimensions must fit u16')
    flat=[v for r in rows for v in r]
    packed=bytearray((w*h+1)//2)
    for i,v in enumerate(flat):
        packed[i//2] |= (v<<4) if (i&1) else v
    tex=tex_path.read_bytes()
    if not tex or len(tex)%4096:
        raise ValueError('texture payload must be non-empty multiple of 4096')
    count=len(tex)//4096
    if count>65535:
        raise ValueError('texture count >65535')
    header=struct.pack('<4sBHHHIIH', b'L3DP', 2, w, h, count, len(packed), len(tex), 0)
    out_path.write_bytes(header+packed+tex)
    print(f'wrote {out_path}: {w}x{h} map, {count} texture(s), {len(header)+len(packed)+len(tex)} bytes')
    return 0
if __name__=='__main__': raise SystemExit(main())
