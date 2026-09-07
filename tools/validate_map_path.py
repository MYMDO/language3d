#!/usr/bin/env python3
"""Validate that start and target are open and connected in map.txt.

Usage: validate_map_path.py MAP_TXT START_X START_Y TARGET_X TARGET_Y
"""
from collections import deque
from pathlib import Path
import sys

def main() -> int:
    if len(sys.argv) != 6:
        print(__doc__.strip(), file=sys.stderr); return 2
    path=Path(sys.argv[1]); sx,sy,tx,ty=map(int,sys.argv[2:])
    rows=[''.join(r.split()) for r in path.read_text().splitlines() if r.strip()]
    if not rows or len({len(r) for r in rows}) != 1:
        print('invalid: non-rectangular map', file=sys.stderr); return 1
    h,w=len(rows),len(rows[0])
    for x,y,name in [(sx,sy,'start'),(tx,ty,'target')]:
        if not (0<=x<w and 0<=y<h) or rows[y][x]=='1':
            print(f'invalid: {name} is blocked/out of bounds', file=sys.stderr); return 1
    q=deque([(sx,sy)]); seen={(sx,sy)}
    while q:
        x,y=q.popleft()
        if (x,y)==(tx,ty):
            print(f'MAP PATH OK: {w}x{h}, start=({sx},{sy}), target=({tx},{ty}), visited={len(seen)}')
            return 0
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            nx,ny=x+dx,y+dy
            if 0<=nx<w and 0<=ny<h and rows[ny][nx]=='0' and (nx,ny) not in seen:
                # The supplied default maze has a short guaranteed route to the target.
                # Keep the validator bounded so arbitrary enormous connected maps do not
                # turn the normal test suite into a whole-world flood fill.
                if len(seen) >= 1_000_000:
                    print('invalid: path search exceeded 1,000,000 nodes', file=sys.stderr); return 1
                seen.add((nx,ny)); q.append((nx,ny))
    print('invalid: target is unreachable from start', file=sys.stderr); return 1
if __name__=='__main__': raise SystemExit(main())
