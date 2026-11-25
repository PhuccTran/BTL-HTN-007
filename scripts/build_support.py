"""Small cross-platform helpers; requires Python 3.8+ and no packages."""
import argparse
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]


def build_path(value):
    candidate = ROOT / value
    resolved = candidate.resolve()
    build = ROOT / 'build'
    if build.is_symlink() or build.resolve() != build:
        raise ValueError('Refusing to use a redirected build directory')
    if resolved != build and build not in resolved.parents:
        raise ValueError(f'Output path must stay inside build/: {resolved!s} (root: {build!s})')
    return resolved


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['help', 'check', 'mkdir', 'stamp', 'show', 'clean'])
    parser.add_argument('--tool', action='append', default=[])
    parser.add_argument('--required', action='append', default=[])
    parser.add_argument('--entry', action='append', default=[])
    parser.add_argument('--output')
    args = parser.parse_args()
    if args.action == 'help':
        print('''STM32F411CE + ST-Link / FreeRTOS
make                 Build ELF + HEX + BIN + MAP
make check           Check compiler and source files
make flash           Build and flash with st-flash, reset and run
make reset           Reset the connected MCU with st-flash
make debug-server    Start st-util GDB server (terminal 1)
make debug           Connect GDB (terminal 2; flash first)
make size            Show code/data/BSS size
make disasm          Write build/<target>.lst
make hex / make bin  Build a selected image format
make sources         List the exact compiled source files
make print-config    Show board and tool configuration
make clean           Remove only this project's build/ directory
make rebuild         Clean, then build sequentially

Configuration: project.mk; local overrides: project.local.mk
Windows: mingw32-make may be used in place of make.
See README.md for installation, wiring, adding drivers and tasks.''')
    elif args.action == 'check':
        errors = [f'Tool not found: {tool}. Add its bin directory to PATH.'
                  for tool in args.tool if not shutil.which(tool)]
        errors += [f'File not found: {path}' for path in args.required
                   if not (ROOT / path).is_file()]
        if errors:
            print('\n'.join(errors), file=sys.stderr)
            return 1
        print('Check OK')
    elif args.action == 'mkdir':
        build_path(args.output).mkdir(parents=True, exist_ok=True)
    elif args.action == 'stamp':
        path = build_path(args.output)
        content = '\n'.join(args.entry) + '\n'
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_text(encoding='utf-8') != content:
            path.write_text(content, encoding='utf-8')
    elif args.action == 'show':
        print('\n'.join(args.entry))
    elif args.action == 'clean':
        path = build_path('build')
        if path.exists():
            shutil.rmtree(path)
        print('Cleaned build/')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as exc:
        print(f'Build helper: {exc}', file=sys.stderr)
        sys.exit(1)
