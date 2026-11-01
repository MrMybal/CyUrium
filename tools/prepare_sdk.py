# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Build a private, version/hash-pinned SDK runtime; never modify system Python."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT/'sdk/runtime.lock.json').read_text(encoding='utf-8'))


def digest(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()


def prepare(destination):
    if sys.platform != 'win32':
        raise RuntimeError('Private SDK packaging currently validated only for Windows x64')
    cache = ROOT/'.cache/sdk'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache/Path(LOCK['python']['url']).name
    if not archive.exists():
        urllib.request.urlretrieve(LOCK['python']['url'], archive)
    if digest(archive) != LOCK['python']['sha256']:
        raise RuntimeError('Embedded Python SHA256 mismatch')
    core = ROOT/'sdk/packages'/LOCK['cytoolsCore']['wheel']
    if digest(core) != LOCK['cytoolsCore']['sha256']:
        raise RuntimeError('CyToolsCore wheel SHA256 mismatch')
    wheels = cache/'wheels'
    wheels.mkdir(exist_ok=True)
    frozen = ROOT/'sdk/requirements.lock.json'
    if not frozen.exists():
        subprocess.run([sys.executable, '-m', 'pip', '--disable-pip-version-check', 'download',
                        '--only-binary=:all:', '--platform', 'win_amd64', '--python-version', '3.14',
                        '--dest', str(wheels), str(core)+'[mcp]'], check=True)
        entries = {p.name:digest(p) for p in sorted(wheels.glob('*.whl'))}
        frozen.write_text(json.dumps(entries, indent=2)+'\n', encoding='utf-8')
    entries = json.loads(frozen.read_text(encoding='utf-8'))
    for name, sha in entries.items():
        wheel = wheels/name
        if not wheel.exists():
            if name == core.name:
                shutil.copy2(core, wheel)
            else:
                package, version = name.split('-')[:2]
                subprocess.run([sys.executable, '-m', 'pip', '--disable-pip-version-check', 'download',
                                '--no-deps', '--only-binary=:all:', '--platform', 'win_amd64',
                                '--python-version', '3.14', '--dest', str(wheels), package+'=='+version], check=True)
        if digest(wheel) != sha:
            raise RuntimeError('SDK dependency SHA256 mismatch: '+name)
    destination.mkdir(parents=True, exist_ok=True)
    private = destination/'sdk/python'
    private.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        z.extractall(private)
    # No user site, PYTHONPATH, pip, startup scripts or global installation.
    (private/'python314._pth').write_text('python314.zip\n.\nLib/site-packages\nLib/site-packages/win32\nLib/site-packages/win32/lib\nLib/site-packages/Pythonwin\n..\n../vendor\n', encoding='utf-8')
    site = private/'Lib/site-packages'
    notices = destination/'LICENCES/SDK'
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(private/'LICENSE.txt', notices/'Python.txt')
    for name in entries:
        with zipfile.ZipFile(wheels/name) as z:
            for info in z.infolist():
                if '.data/' in info.filename:
                    continue  # CLI scripts/docs are not needed by this embedded adapter.
                z.extract(info, site)
                if '.dist-info/' in info.filename and ('license' in info.filename.lower() or 'notice' in info.filename.lower()):
                    target = notices/info.filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(z.read(info))
    for source in (ROOT/'sdk').glob('*.py'):
        shutil.copy2(source, destination/'sdk'/source.name)
    provider_lock=json.loads((ROOT/'sdk/providers.lock.json').read_text(encoding='utf-8'))
    for name,sha in provider_lock['files'].items():
        source=ROOT/'sdk/vendor'/name
        if digest(source)!=sha: raise RuntimeError('Provider source changed: '+name)
        target=destination/'sdk/vendor'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
    for dll in (site/'pywin32_system32').glob('*.dll'):
        shutil.copy2(dll, private/dll.name)
    for name in ('CyTool.json', 'runtime.lock.json', 'requirements.lock.json','providers.lock.json'):
        shutil.copy2(ROOT/'sdk'/name, destination/'sdk'/name)
    shutil.copytree(ROOT/'LICENCES', destination/'LICENCES', dirs_exist_ok=True)
    subprocess.run([str(private/'python.exe'), '-I', '-B', '-c',
                    "from importlib.metadata import version,metadata; assert version('cytools-core')=='0.10.1'; assert metadata('cytools-core')['License-Expression']=='MIT'; assert version('mcp')=='1.30.0'; import cytools_core, mcp; print('Private CyToolsCore / MCP runtime ready')"], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', type=Path, default=ROOT/'.cache/sdk/bundle')
    prepare(parser.parse_args().destination)
