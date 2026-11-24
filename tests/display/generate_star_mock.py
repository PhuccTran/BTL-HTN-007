#!/usr/bin/env python3
"""Tạo đường đi DỮ LIỆU GIẢ offline; không đưa thuật toán game vào firmware.
BFS ở mức ô lưới tránh tường/bẫy, cổng chỉ đi vào khi đã lấy đủ sao.
"""
from pathlib import Path
from collections import deque
import json,re
root=Path(__file__).resolve().parents[2]
fixtures=root/'tests/display/fixtures/star_maze'
source=(root/'app/src/star_maze_levels.c').read_text()
array=source.split('= {',1)[1].split('};',1)[0]
array=re.sub(r'/\*.*?\*/','',array,flags=re.S)
numbers=[int(n) for n in re.findall(r'\b\d+\b',array)]
assert len(numbers)==5*12*12
lines=['/* Sinh tự động bởi generate_star_mock.py; chỉ dùng trong TEST. */',
       '#ifndef STAR_MAZE_MOCK_PATHS_H','#define STAR_MAZE_MOCK_PATHS_H','#include <stdint.h>',
       'typedef struct { uint8_t cell; uint16_t mask; } StarMockPoint_t;',
       'typedef struct { const StarMockPoint_t *points; uint8_t count; } StarMockPath_t;']
results=[]
metadata=json.loads((fixtures/'KIEM_TRA_5_MAN.json').read_text())
def declared_counts(name):
    value=re.search(rf'{name}\[STAR_MAZE_NUM_LEVELS\]\s*=\s*\{{([^}}]+)\}}',source)
    assert value is not None,name
    return [int(n) for n in re.findall(r'\d+',value[1])]
star_counts=declared_counts('star_maze_star_count')
trap_counts=declared_counts('star_maze_trap_count')
for level in range(1,6):
    raw=(fixtures/f'MAN_{level}_12x12.txt').read_text().splitlines()
    assert len(raw)==12 and all(len(r)==12 and set(r)<=set('012345') for r in raw)
    cells=[int(c) for row in raw for c in row]
    assert cells==numbers[(level-1)*144:level*144],f'C data mismatch level {level}'
    assert cells.count(2)==cells.count(3)==1
    start=cells.index(2); goal=cells.index(3)
    star_indices=[i for i,c in enumerate(cells) if c==4]
    bits={cell:1<<k for k,cell in enumerate(star_indices)}; full=(1<<len(bits))-1
    begin=(start,0); q=deque([begin]); parents={begin:None}; end=None
    while q:
        cell,mask=q.popleft()
        if cell==goal and mask==full: end=(cell,mask); break
        row,col=divmod(cell,12)
        for dr,dc in [(-1,0),(1,0),(0,-1),(0,1)]:
            rr,cc=row+dr,col+dc
            if not (0<=rr<12 and 0<=cc<12): continue
            nxt=rr*12+cc
            if cells[nxt] in (1,5) or (nxt==goal and mask!=full): continue
            key=(nxt,mask|bits.get(nxt,0))
            if key not in parents: parents[key]=(cell,mask);q.append(key)
    assert end is not None,f'No trap-free all-stars path level {level}'
    path=[]; p=end
    while p is not None: path.append(p);p=parents[p]
    path.reverse()
    assert len(bits)==star_counts[level-1]==metadata[level-1]['so_sao']
    assert cells.count(5)==trap_counts[level-1]==metadata[level-1]['so_bay']
    assert len(path)-1==metadata[level-1]['so_o_duong_di_an_het_sao_den_dich']
    lines.append(f'static const StarMockPoint_t s_star_path_{level}[]={{')
    lines.extend(f'    {{{cell}U,0x{mask:04X}U}},' for cell,mask in path)
    lines.append('};')
    results.append({'level':level,'stars':len(bits),'traps':cells.count(5),'route_steps':len(path)-1})
lines.append('static const StarMockPath_t s_star_mock_paths[5]={')
for level in range(1,6):
    lines.append(f'    {{s_star_path_{level},(uint8_t)(sizeof(s_star_path_{level})/sizeof(s_star_path_{level}[0]))}},')
lines+=['};','#endif','']
def write_if_changed(path,text):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_text()!=text: path.write_text(text)
write_if_changed(root/'tests/display/star_maze_mock_paths.h','\n'.join(lines))
write_if_changed(root/'build/display-unit/map-validation.json',json.dumps(results,indent=2)+'\n')
print(json.dumps(results))
