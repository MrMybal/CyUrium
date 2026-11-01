# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Bundle unchanged, pinned WebExtensions and their corresponding sources."""
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def pinned_download(entry):
    path = ROOT/'.cache/downloads'/entry['file']
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        # Download to a sibling so a network interruption cannot poison the cache.
        partial = path.with_suffix(path.suffix+'.part')
        urllib.request.urlretrieve(entry['url'], partial)
        if hashlib.sha256(partial.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Bundled extension download checksum mismatch: '+entry['file'])
        partial.replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
        raise RuntimeError('Bundled extension checksum mismatch: '+entry['file'])
    return path


def prepare(app):
    lock = json.loads((ROOT/'engine/bundled-extensions.lock.json').read_text(encoding='utf-8'))
    if lock['schema'] != 1:
        raise RuntimeError('Unsupported bundled extension lock')
    target = app/'extensions'
    target.mkdir(parents=True, exist_ok=True)
    generated = []
    for addon in lock['extensions']:
        package = pinned_download(addon)
        with zipfile.ZipFile(package) as z:
            manifest = json.loads(z.read('manifest.json'))
            if (manifest['browser_specific_settings']['gecko']['id'] != addon['id'] or
                    manifest['version'] != addon['version'] or 'META-INF/mozilla.rsa' not in z.namelist()):
                raise RuntimeError('Bundled extension identity/version/signature envelope mismatch')
            (target/(addon['id']+'-LICENSE.txt')).write_bytes(z.read('LICENSE.txt'))
            generated.append('extensions/'+addon['id']+'-LICENSE.txt')
        # Preserve Mozilla's signature and the extension's complete embedded
        # licences, filter lists and third-party notices without rewriting it.
        shutil.copy2(package, target/addon['file'])
        generated.append('extensions/'+addon['file'])
        source = pinned_download(addon['source'])
        (target/'sources').mkdir(exist_ok=True)
        shutil.copy2(source, target/'sources'/source.name)
        generated.append('extensions/sources/'+source.name)
    shutil.copy2(ROOT/'engine/bundled-extensions.lock.json', target/'bundled-extensions.json')
    generated.append('extensions/bundled-extensions.json')
    notice = ROOT/'engine/BUNDLED_EXTENSIONS.md'
    shutil.copy2(notice, target/notice.name)
    generated.append('extensions/'+notice.name)
    return generated
