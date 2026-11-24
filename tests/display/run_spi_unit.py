#!/usr/bin/env python3
"""Chạy backend CMSIS SPI/DMA thật trên ARM; ngoại vi được mô hình hóa."""
from pathlib import Path
import json,struct,subprocess
from fetch_model_deps import ensure_cmsis
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_CPU_ARM_CORTEX_M4
root=Path(__file__).resolve().parents[2]
out=root/'build/display-unit'; out.mkdir(parents=True,exist_ok=True)
elf=out/'spi.elf'
cmsis=ensure_cmsis()
subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Og','-g',
 '-std=gnu11','-Wall','-Wextra','-Werror','-DSTM32F411xE',
 '-Itests/display/mock_rtos','-Ihal/inc','-Ibsp','-I'+str(cmsis/'Core'),
 '-I'+str(cmsis/'Device'),'hal/src/hal_spi.c','tests/display/unit_spi.c',
 '-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Ttests/display/unit.ld','-o',str(elf)],cwd=root,check=True)
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-n',str(elf)],text=True).splitlines():
 p=line.split()
 if len(p)==3: symbols[p[2]]=int(p[0],16)
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS); u.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
for address,size in [(0x10000,0x100000),(0x20000000,0x100000),(0x40000000,0x30000),(0xE0000000,0x100000)]: u.mem_map(address,size)
data=elf.read_bytes(); off=struct.unpack_from('<I',data,28)[0]; size,count=struct.unpack_from('<HH',data,42)
for i in range(count):
 typ,fileoff,vaddr,_,filesz,_,_,_=struct.unpack_from('<8I',data,off+i*size)
 if typ==1 and filesz: u.mem_write(vaddr,data[fileoff:fileoff+filesz])
u.reg_write(UC_ARM_REG_SP,0x200FF000); u.reg_write(UC_ARM_REG_LR,symbols['test_done']|1)
done=[False]; pending=[None]
def write(machine,access,address,size,value,user):
 if address==0x40026458 and struct.unpack('<I',machine.mem_read(symbols['test_stuck_dma'],4))[0] and not value&1:
  pending[0]=value|1
def code(machine,address,size,user):
 if pending[0] is not None:
  machine.mem_write(0x40026458,struct.pack('<I',pending[0])); pending[0]=None
 if address==symbols['test_done']:
  done[0]=True; machine.emu_stop()
u.hook_add(UC_HOOK_MEM_WRITE,write); u.hook_add(UC_HOOK_CODE,code)
u.emu_start(symbols['test_entry']|1,0,count=4000000)
if not done[0]: raise RuntimeError('SPI test instruction budget exceeded')
def get(name): return struct.unpack('<I',u.mem_read(symbols[name],4))[0]
if get('test_failure'): raise AssertionError(f'unit_spi.c:{get("test_failure")}')
result={'status':'PASS','checks':get('test_checks'),'target':'Actual CMSIS SPI/DMA code; peripheral and RTOS models'}
(out/'spi-result.json').write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps(result))
