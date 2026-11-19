# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Package the source-built native CyMCPSDK companion, without a Python runtime."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import time

ROOT=Path(__file__).resolve().parents[1]


def prepare(destination, build_dir=None):
    destination=Path(destination).resolve()
    lock=json.loads((ROOT/'sdk/cymcpsdk.lock.json').read_text(encoding='utf-8'))
    for name,digest in lock['files'].items():
        if hashlib.sha256((ROOT/'third_party/cymcpsdk'/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Pinned CyMCPSDK source differs: '+name)
    build_dir=Path(build_dir) if build_dir else ROOT/'build/win32-release'
    executable=build_dir/('CyUriumCLI.exe' if __import__('sys').platform=='win32' else 'CyUriumCLI')
    if not executable.is_file(): raise RuntimeError('Build the native CyUriumCLI target before packaging')
    # Preserve old local runtime copies. Their user profiles live outside this package.
    legacy=[]
    if (destination/'sdk/python').exists(): legacy.append(destination/'sdk')
    if (destination/'LICENCES/SDK').exists(): legacy.append(destination/'LICENCES/SDK')
    for name in ('CyToolsCore-MIT.txt','CyUEMCPSDK-MIT.txt'):
        if (destination/'LICENCES'/name).exists(): legacy.append(destination/'LICENCES'/name)
    if legacy:
        backup=(ROOT/'.cache/legacy-sdk-packages'/str(time.time_ns())).resolve()
        if not backup.is_relative_to(ROOT/'.cache') or not destination.is_relative_to(ROOT/'dist'):
            raise RuntimeError('Legacy package migration must stay within workspace dist/cache')
        backup.mkdir(parents=True)
        for index,source in enumerate(legacy):
            if not source.resolve().is_relative_to(destination):raise RuntimeError('Unexpected legacy runtime link')
            shutil.move(str(source),str(backup/(str(index)+'-'+source.name)))
    destination.mkdir(parents=True,exist_ok=True)
    shutil.copy2(executable,destination/executable.name)
    (destination/'sdk').mkdir(exist_ok=True)
    for name in ('operations.json','cymcpsdk.lock.json'): shutil.copy2(ROOT/'sdk'/name,destination/'sdk'/name)
    licenses=destination/'licenses';licenses.mkdir(exist_ok=True)
    for source,name in [('LICENSE','CyMCPSDK-MIT.txt'),('THIRD_PARTY_NOTICES.md','CyMCPSDK-third-party.md'),('third_party/httplib/LICENSE','cpp-httplib-MIT.txt'),('third_party/picosha2/LICENSE','PicoSHA2-MIT.txt')]:
        shutil.copy2(ROOT/'third_party/cymcpsdk'/source,licenses/name)
    (destination/'LICENCES').mkdir(exist_ok=True)
    for path in (ROOT/'LICENCES').iterdir():
        if path.is_file() and path.name not in ('CyToolsCore-MIT.txt','CyUEMCPSDK-MIT.txt'):shutil.copy2(path,destination/'LICENCES'/path.name)
    print('Native CyMCPSDK',lock['version'],lock['revision'],flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--destination',type=Path,default=ROOT/'dist/windows/CyUrium')
    parser.add_argument('--build-dir',type=Path)
    args=parser.parse_args();prepare(args.destination,args.build_dir)
