#!/usr/bin/env python3
"""Build firmware test riêng cho phần màn hình; không sửa cấu hình RTOS chung."""
import argparse
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser()
p.add_argument('mode',choices=['basic','mock'])
p.add_argument('--polling',action='store_true')
p.add_argument('--x-offset',type=int,default=0)
p.add_argument('--y-offset',type=int,default=0)
p.add_argument('--madctl',type=lambda s:int(s,0),default=0xC8)
p.add_argument('--invert',action='store_true')
p.add_argument('--reset-defaults',action='store_true',help='Tắt profile frame/power/gamma tham chiếu')
args=p.parse_args()
if not 0<=args.x_offset<=4 or not 0<=args.y_offset<=2 or args.madctl&0x20:
    p.error('Offset hợp lệ x=0..4, y=0..2; portrait không bật bit MV 0x20')
root=Path(__file__).resolve().parents[2]
if args.mode=='mock':
    subprocess.run([sys.executable,str(root/'tests/display/generate_star_mock.py')],cwd=root,check=True)
suffix='-polling' if args.polling else ''
name='display-'+args.mode+suffix
flags=['-Werror','-fstack-usage',f'-DDISPLAY_TEST_BASIC={int(args.mode=="basic")}',
       f'-DBSP_DISPLAY_USE_DMA={int(not args.polling)}',f'-DBSP_DISPLAY_X_OFFSET={args.x_offset}',
       f'-DBSP_DISPLAY_Y_OFFSET={args.y_offset}',f'-DBSP_DISPLAY_MADCTL={args.madctl}',
       f'-DBSP_DISPLAY_INVERT={int(args.invert)}',f'-DBSP_DISPLAY_PANEL_PROFILE={int(not args.reset_defaults)}']
sources='tests/display/kit_main.c'+(' tests/display/mock_display.c' if args.mode=='mock' else '')
subprocess.run(['make','-j4',f'BUILD_DIR=build/{name}',f'TARGET={name}',
                'ENTRY_SOURCE=tests/display/kit_main.c',f'EXTRA_C_SOURCES={sources}',
                'EXTRA_INCLUDE_DIRS=middlewares/CMSIS-F411/Core middlewares/CMSIS-F411/Device tests/display',
                'EXTRA_CFLAGS='+' '.join(flags)],cwd=root,check=True)
print(f'Firmware test: {root}/build/{name}/{name}.bin')
