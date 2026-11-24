#!/usr/bin/env python3
"""Kiểm tra cấp phát startup với kernel FreeRTOS thật, dừng trước context switch.
Không chạy clock SystemInit, scheduler, ISR hoặc giao tiếp điện trên board.
"""
from pathlib import Path
import argparse,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_PC,UC_CPU_ARM_CORTEX_M4
p=argparse.ArgumentParser(); p.add_argument('elf',type=Path); a=p.parse_args()
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(a.elf)],text=True).splitlines():
 parts=line.split()
 if len(parts)==3: symbols[parts[2]]=int(parts[0],16)
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS); u.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
for address,size in [(0x08000000,0x100000),(0x20000000,0x20000),(0x40000000,0x30000),(0xE0000000,0x100000)]: u.mem_map(address,size)
data=a.elf.read_bytes(); off=struct.unpack_from('<I',data,28)[0]; size,count=struct.unpack_from('<HH',data,42)
for i in range(count):
 typ,fileoff,vaddr,_,filesz,_,_,_=struct.unpack_from('<8I',data,off+i*size)
 if typ==1 and filesz: u.mem_write(vaddr,data[fileoff:fileoff+filesz])
u.reg_write(UC_ARM_REG_SP,0x20020000); u.reg_write(UC_ARM_REG_LR,symbols['vAssertCalled']|1)
hit=[None]
def stop(machine,address,size,user):
 hit[0]=address; machine.emu_stop()
for name in ['xPortStartScheduler','vAssertCalled']:
 u.hook_add(UC_HOOK_CODE,stop,begin=symbols[name],end=symbols[name])
u.emu_start(symbols['main']|1,0,count=3000000)
if hit[0]!=symbols['xPortStartScheduler']:
 addr=u.reg_read(UC_ARM_REG_R0)
 try: detail=bytes(u.mem_read(addr,160)).split(b'\0')[0].decode()
 except Exception: detail=f'R0={addr:#x}'
 raise AssertionError(f'Startup failed PC={u.reg_read(UC_ARM_REG_PC):#x}: {detail}:{u.reg_read(UC_ARM_REG_R1)}')
free=struct.unpack('<I',u.mem_read(symbols['xFreeBytesRemaining'],4))[0]
result={'status':'PASS','elf':str(a.elf),'free_heap_before_context_switch':free,
        'scope':'Actual kernel allocations including Idle/Timer; no task execution or electrical validation'}
print(json.dumps(result,ensure_ascii=False))
a.elf.with_suffix('.startup.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
