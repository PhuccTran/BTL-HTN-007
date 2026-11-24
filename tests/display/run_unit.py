#!/usr/bin/env python3
"""Không cần compiler host: build ARM rồi chạy với Python package unicorn.
python3 -m pip install unicorn
python3 tests/display/run_unit.py
Không giả lập timing điện, DMA hay scheduler FreeRTOS thật.
"""
from pathlib import Path
import json, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR

root=Path(__file__).resolve().parents[2]
out=root/'build/display-unit'; out.mkdir(parents=True,exist_ok=True)
elf=out/'unit.elf'
cmd=['arm-none-eabi-gcc','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Og','-g',
 '-std=gnu11','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',
 '-Itests/display/mock','-Iapp/inc','-Ihal/inc','hal/src/hal_st7735.c',
 'app/src/display_render.c','app/src/star_maze_levels.c','app/src/task_display.c','tests/display/unit_display.c',
 '-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Ttests/display/unit.ld',
 '-Wl,--gc-sections','-o',str(elf)]
subprocess.run(cmd,cwd=root,check=True)
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(elf)],text=True).splitlines():
    parts=line.split()
    if len(parts)==3: symbols[parts[2]]=int(parts[0],16)
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x10000,0x100000); uc.mem_map(0x20000000,0x100000)
data=elf.read_bytes()
phoff=struct.unpack_from('<I',data,28)[0]
phsize,phnum=struct.unpack_from('<HH',data,42)
for i in range(phnum):
    typ,offset,vaddr,paddr,filesz,memsz,flags,align=struct.unpack_from('<8I',data,phoff+i*phsize)
    if typ==1 and filesz: uc.mem_write(vaddr,data[offset:offset+filesz])
uc.reg_write(UC_ARM_REG_SP,0x200FF000); uc.reg_write(UC_ARM_REG_LR,symbols['test_done']|1)
done=[False]
def stop(machine,address,size,user):
    done[0]=True; machine.emu_stop()
uc.hook_add(UC_HOOK_CODE,stop,begin=symbols['test_done'],end=symbols['test_done'])
uc.emu_start(symbols['test_entry']|1,0,count=30000000)
if not done[0]: raise RuntimeError('Instruction budget exceeded')
def u32(name): return struct.unpack('<I',uc.mem_read(symbols[name],4))[0]
if u32('test_failure'): raise AssertionError(f'unit_display.c:{u32("test_failure")}')
result={'checks':u32('test_checks'),'status':'PASS','target':'ARM Thumb under Unicorn; RTOS/SPI/GPIO mocked'}
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
# PPM chuẩn, không cần thư viện ảnh để xuất ảnh từ kết quả C.
raw=bytes(uc.mem_read(symbols['test_snapshots'],8*128*160*2))
for i,name in enumerate(['scene','notice','resume','level-1','level-2','level-3','level-4','level-5']):
    pixels=struct.unpack_from('<20480H',raw,i*40960)
    rgb=bytearray()
    for pixel in pixels:
        v=255 if pixel==65535 else 0; rgb.extend((v,v,v))
    (out/f'{name}.ppm').write_bytes(b'P6\n128 160\n255\n'+rgb)
print(json.dumps(result))
