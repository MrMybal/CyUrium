# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Package a rebuilt Windows browser as a portable ZIP and per-user installer.

This does not create a publisher key or a signed automatic-update manifest.
Use release.py for the separately configured signed updater channel.
"""
from pathlib import Path
import argparse
import configparser
import json
import re
import subprocess
import tempfile
import zipfile

from check_package_paths import check_package_paths
from runtime_provenance import file_hash, stage_package, verify_runtime

ROOT = Path(__file__).resolve().parents[1]


def validate_package(source, version):
    app = configparser.ConfigParser()
    app.read(source / 'engine/app/application.ini', encoding='utf-8')
    if app.get('App', 'Version', fallback='') != version:
        raise ValueError('The built browser and requested version differ')
    required = ('CyUrium.exe', 'LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md',
                'sdk/python/python.exe', 'assets/branding/urium.ico', 'LICENCES/Inno-Setup.txt')
    if any(not (source / name).is_file() for name in required):
        raise ValueError('Incomplete Windows package or license notices')
    forbidden_parts = {'profile', 'profiles', '.runtime', '__pycache__', '.git'}
    forbidden_suffixes = {'.log', '.dmp', '.pdb', '.pem', '.key', '.pfx', '.p12', '.sqlite', '.db'}
    for path in source.rglob('*'):
        if any(part.lower() in forbidden_parts for part in path.relative_to(source).parts):
            raise ValueError('Profile or local-state directory in package')
        public_ca = path.relative_to(source).as_posix() == 'sdk/python/Lib/site-packages/certifi/cacert.pem'
        if path.is_file() and path.suffix.lower() in forbidden_suffixes and not public_ca:
            raise ValueError('Local-state or private-key file in package: ' + path.name)
    check_package_paths(source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'dist/windows/CyUrium')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/releases')
    parser.add_argument('--iscc', type=Path, required=True, help='Official Inno Setup 7 ISCC.exe')
    args = parser.parse_args()
    version = re.search(r'project\(CyUrium VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
    output = args.output.resolve()
    if output.is_relative_to(args.source.resolve()):
        parser.error('Release output must be outside the browser package')
    if not args.iscc.is_file():
        parser.error('Inno Setup compiler not found')
    names = [f'CyUrium-{version}-windows-x64.zip', f'CyUrium-{version}-windows-x64-setup.exe', 'SHA256SUMS']
    output.mkdir(parents=True, exist_ok=True)
    if any((output / name).exists() for name in names):
        parser.error('Output already contains a release; use a new directory')
    lock = json.loads((ROOT / 'engine/runtime.lock.json').read_text(encoding='utf-8'))
    with tempfile.TemporaryDirectory(prefix='cyurium-package-', dir=output) as temp:
        source = stage_package(args.source, Path(temp) / 'CyUrium', lock['sha512'])
        validate_package(source, version)
        files = {p.relative_to(source).as_posix(): file_hash(p) for p in sorted(source.rglob('*')) if p.is_file() and p.relative_to(source).as_posix() != 'package-files.json'}
        (source / 'package-files.json').write_text(json.dumps(files, separators=(',', ':')), encoding='utf-8')
        archive = output / names[0]
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as package:
            for path in sorted(source.rglob('*')):
                if path.is_file():
                    package.write(path, path.relative_to(source).as_posix())
        subprocess.run([str(args.iscc.resolve()), '/Qp', '/DPackageDir=' + str(source),
                        '/DOutputPath=' + str(output), '/DAppVersion=' + version,
                        str(ROOT / 'packaging/windows/CyUrium.iss')], check=True)
        verify_runtime(source / 'engine', lock['sha512'])
        for name, digest in files.items():
            if file_hash(source / name) != digest:
                raise ValueError('Browser package changed during installer creation: ' + name)
        (output / 'SHA256SUMS').write_text(''.join(f'{file_hash(output / name)}  {name}\n' for name in names[:2]), encoding='utf-8')
    print('Windows installer and portable archive:', output)


if __name__ == '__main__':
    main()
