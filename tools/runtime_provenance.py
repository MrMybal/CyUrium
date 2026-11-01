# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""File provenance gates for the pinned Gecko archive and release packages."""
from pathlib import Path
import hashlib
import json
import stat
import shutil
import subprocess
import zlib

def file_hash(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            h.update(chunk)
    return h.hexdigest()

def safe_name(name):
    return name and '\\' not in name and ':' not in name and all(p not in ('', '.', '..') for p in name.split('/'))

def archive_files(seven, archive, pattern='*'):
    result = subprocess.run([seven, 'l', '-slt', '-ba', str(archive), pattern], capture_output=True)
    text = result.stdout.decode('utf-8', errors='replace')
    error = result.stderr.decode('utf-8', errors='replace')
    # -ba suppresses the known optimized-ZIP header warning. Every listed file
    # is still checked against its CRC and size before any bytes are used.
    if result.returncode not in (0, 1) and not (archive.name == 'omni.ja' and result.returncode == 2 and 'CRC Failed' not in text + error and 'Data Error' not in text + error):
        raise RuntimeError('Cannot list pinned archive: ' + error[-500:])
    files, seen = {}, set()
    for block in text.replace('\r\n', '\n').split('\n\n'):
        entry = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
        if not entry.get('Path') or entry.get('Folder') == '+' or entry.get('Attributes', '').startswith('D'):
            continue
        name = entry['Path'].replace('\\', '/')
        if not safe_name(name) or name.lower() in seen:
            raise RuntimeError('Ambiguous archive path: ' + name)
        if not entry.get('CRC') or not entry.get('Size', '').isdigit():
            raise RuntimeError('Missing archive integrity metadata: ' + name)
        files[name] = dict(size=int(entry['Size']), crc=entry['CRC'].lower())
        seen.add(name.lower())
    if not files:
        raise RuntimeError('Empty pinned archive listing')
    return files

def verify_extracted(folder, entries):
    """Ignore foreign additions; verify each approved file against archive CRC/size."""
    hashes = {}
    for name, expected in entries.items():
        path = folder / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size != expected['size']:
            raise RuntimeError('Extracted runtime file differs from the pinned archive: ' + name)
        crc, digest = 0, hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1048576), b''):
                crc = zlib.crc32(chunk, crc)
                digest.update(chunk)
        if f'{crc:08x}' != expected['crc']:
            raise RuntimeError('Extracted runtime file CRC differs from the pinned archive: ' + name)
        hashes[name] = digest.hexdigest()
    return hashes

def verify_runtime(folder, archive_hash, allow_foreign=False):
    manifest = folder / 'runtime-files.json'
    if not manifest.is_file() or manifest.stat().st_size > 4 * 1024 * 1024:
        raise RuntimeError('Missing Gecko file provenance; rebuild before packaging')
    data = json.loads(manifest.read_text(encoding='utf-8'))
    files = data.get('files')
    if data.get('version') != 1 or data.get('archive_sha512') != archive_hash or not isinstance(files, dict) or not files or len(files) > 50000:
        raise RuntimeError('Invalid Gecko provenance manifest')
    if not {'firefox.exe','xul.dll','app/application.ini','app/omni.ja','runtime.lock.json'}.issubset(files):
        raise RuntimeError('Incomplete Gecko provenance manifest')
    expected = set(files) | {'runtime-files.json'}
    for name, digest in files.items():
        if not safe_name(name) or not isinstance(digest, str) or len(digest) != 64:
            raise RuntimeError('Invalid runtime file entry')
        path = folder / name
        if not path.is_file() or path.is_symlink() or file_hash(path) != digest:
            raise RuntimeError('Gecko file modified after assembly: ' + name)
    for path in folder.rglob('*'):
        if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0):
            raise RuntimeError('Link in runtime package: ' + str(path))
        if not allow_foreign and path.is_file() and path.relative_to(folder).as_posix() not in expected:
            raise RuntimeError('Foreign file in runtime package: ' + path.relative_to(folder).as_posix() + '. Publish from a clean staging directory or CI runner.')
    return data

def stage_package(source, destination, archive_hash):
    source, destination = source.resolve(), destination.resolve()
    if destination.exists() or destination.is_relative_to(source) or source.is_relative_to(destination):
        raise RuntimeError('Staging needs a new, separate output directory')
    data = verify_runtime(source / 'engine', archive_hash, allow_foreign=True)
    for entry in source.iterdir():
        if entry.name not in ('CyUrium.exe', 'assets', 'licenses', 'LICENCES', 'sdk', 'engine', 'package-files.json', 'LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md'):
            raise RuntimeError('Unexpected browser package entry: ' + entry.name)
    destination.mkdir(parents=True)
    for name in ('CyUrium.exe', 'assets', 'licenses', 'LICENCES', 'sdk', 'LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md'):
        path = source / name
        if not path.exists():
            continue
        candidates = [path, *path.rglob('*')] if path.is_dir() else [path]
        for file in candidates:
            if file.is_symlink() or getattr(file.lstat(), 'st_file_attributes', 0) & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0):
                raise RuntimeError('Link in browser package: ' + str(file))
        if path.is_dir():
            shutil.copytree(path, destination / name)
        else:
            shutil.copy2(path, destination / name)
    for name in [*data['files'], 'runtime-files.json']:
        target = destination / 'engine' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / 'engine' / name, target)
    verify_runtime(destination / 'engine', archive_hash)
    return destination
