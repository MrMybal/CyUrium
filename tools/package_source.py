# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Archive committed browser sources with pinned native/extension source archives."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import zipfile

from runtime_provenance import file_hash

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/releases')
    args = parser.parse_args()
    git = ['git', '-c', 'safe.directory=' + ROOT.as_posix()]
    committed = subprocess.check_output(git + ['show', 'HEAD:CMakeLists.txt'], cwd=ROOT, text=True)
    version = re.search(r'project\(CyUrium VERSION ([0-9.]+)', committed).group(1)
    commit = subprocess.check_output(git + ['rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dependencies = subprocess.check_output(git + ['show', 'HEAD:cmake/Dependencies.cmake'], cwd=ROOT, text=True)
    hashes = re.findall(r'URL_HASH SHA256=([a-f0-9]{64})', dependencies)
    if len(hashes) != 2:
        raise ValueError('Review pinned native dependencies before packaging sources')
    archives = [('SDL-3.2.28.tar.gz', hashes[0]), ('imgui-1.92.5-docking.tar.gz', hashes[1])]
    extension_lock = json.loads(subprocess.check_output(git + ['show', 'HEAD:engine/bundled-extensions.lock.json'], cwd=ROOT, text=True))
    # The source archive is pinned separately from the Mozilla-signed XPI.
    for extension in extension_lock['extensions']:
        source = extension['source']
        archives.append((source['file'], source['sha256']))
    for name, expected in archives:
        if file_hash(ROOT / '.cache/downloads' / name) != expected:
            raise ValueError('Source archive hash mismatch: ' + name)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'CyUrium-{version}-sources.zip'
    if target.exists():
        parser.error('Source archive already exists')
    prefix = f'CyUrium-{version}/'
    subprocess.run(git + ['archive', '--format=zip', '--prefix=' + prefix, '--output=' + str(target), 'HEAD'], cwd=ROOT, check=True)
    with zipfile.ZipFile(target, 'a', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, _ in archives:
            archive.write(ROOT / '.cache/downloads' / name, prefix + '.cache/downloads/' + name)
        archive.writestr(prefix + 'SOURCE_BUILD.md', '# Corresponding source\n\nCommit: `' + commit + '`.\n\n'
                          'This archive contains the committed CyUrium sources, vendored libraries and pinned '
                          'SDL, Dear ImGui and uBlock Origin source archives. See README.md for build requirements. '
                          'The native archives are in .cache/downloads/ so the CMake build can use them directly. '
                          'Gecko is an unmodified separate Mozilla runtime; engine/runtime.lock.json records its exact '
                          'source revision and download. The engine assembly recipe and original host sources are included.\n')
    sums = output / 'SHA256SUMS'
    with sums.open('a', encoding='utf-8') as stream:
        stream.write(file_hash(target) + '  ' + target.name + '\n')
    print('Corresponding source archive:', target)


if __name__ == '__main__':
    main()
