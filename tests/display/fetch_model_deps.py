#!/usr/bin/env python3

from pathlib import Path
import hashlib
import json
import os
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).parent / 'fixtures/cmsis_sources.json'

def ensure_cmsis():
    entries = json.loads(MANIFEST.read_text())
    external = os.environ.get('DISPLAY_CMSIS_DIR')
    if external:
        directory = Path(external).expanduser().resolve()
        for entry in entries:
            if entry['path'].endswith('.h') and not (directory / entry['path']).is_file():
                raise FileNotFoundError(directory / entry['path'])
        return directory

    directory = ROOT / 'build/test-deps/CMSIS-F411'
    for entry in entries:
        relative = Path(entry['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Invalid dependency path')
        target = directory / relative
        if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == entry['sha256']:
            continue
        if not entry['url'].startswith(('https://github.com/STMicroelectronics/',
                                        'https://github.com/ARM-software/')):
            raise ValueError('Dependency URL must reference official ST/ARM source')

        url = entry['url'].replace('https://github.com/', 'https://raw.githubusercontent.com/', 1)
        url = url.replace('/blob/', '/', 1)
        request = urllib.request.Request(url, headers={'User-Agent': 'htn007-display-model-tests'})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError(f'CMSIS checksum mismatch: {relative}')
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_name(target.name + '.download')
        temporary.write_bytes(data)
        temporary.replace(target)
        print(f'Cached official CMSIS: {relative}', flush=True)
    return directory

if __name__ == '__main__':
    print(ensure_cmsis())
