#!/usr/bin/env python3
"""Thực thi một vòng mock 5 màn, trap giữ sao, YOU WIN và GAME OVER trên ARM."""
from pathlib import Path
import json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR
root=Path(__file__).resolve().parents[2]
out=root/'build/display-unit'; out.mkdir(parents=True,exist_ok=True)
elf=out/'mock.elf'
subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Og','-g',
 '-std=gnu11','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',
 '-Itests/display/mock','-Itests/display','-Iapp/inc','-Ihal/inc',
 'app/src/display_render.c','app/src/star_maze_levels.c','tests/display/mock_display.c','tests/display/unit_mock.c',
 '-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Ttests/display/unit.ld',
 '-Wl,--gc-sections','-o',str(elf)],cwd=root,check=True)
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(elf)],text=True).splitlines():
 p=line.split()
 if len(p)==3: symbols[p[2]]=int(p[0],16)
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB);u.mem_map(0x10000,0x100000);u.mem_map(0x20000000,0x100000)
data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
for i in range(count):
 typ,fileoff,vaddr,_,filesz,_,_,_=struct.unpack_from('<8I',data,off+i*size)
 if typ==1 and filesz: u.mem_write(vaddr,data[fileoff:fileoff+filesz])
u.reg_write(UC_ARM_REG_SP,0x200FF000);u.reg_write(UC_ARM_REG_LR,symbols['test_done']|1)
done=[False]
def stop(machine,address,size,user): done[0]=True;machine.emu_stop()
u.hook_add(UC_HOOK_CODE,stop,begin=symbols['test_done'],end=symbols['test_done'])
u.emu_start(symbols['test_entry']|1,0,count=20000000)
if not done[0]: raise RuntimeError('Mock instruction budget exceeded')
def get(name):return struct.unpack('<I',u.mem_read(symbols[name],4))[0]
if get('test_failure'): raise AssertionError(f'unit_mock.c:{get("test_failure")}')
result={'status':'PASS','checks':get('test_checks'),'messages':get('test_messages'),
        'wins':get('test_wins'),'game_overs':get('test_game_overs'),'trap_notices':get('test_traps'),
        'scope':'Actual mock task + renderer; RTOS/queue timing/HAL mocked'}
(out/'mock-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
