# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Configure GitHub Releases and build P-256 signed browser packages.

The signing key stays outside the source tree. Only its public key is shipped.
See docs/releases.md for the local and GitHub Actions workflows.
"""
from pathlib import Path
import argparse
import base64
import configparser
from datetime import datetime, timezone
import hashlib
import json
import re
import zipfile
from runtime_provenance import verify_runtime,stage_package
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives import hashes, serialization

ROOT = Path(__file__).resolve().parents[1]
VERSION = r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)'
REPOSITORY = r'[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+'
PLATFORM = 'windows-x64'
MANIFEST = f'latest-{PLATFORM}.json'


def file_hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            digest.update(chunk)
    return digest.hexdigest()


def public_key_text(key):
    return base64.b64encode(key.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)).decode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['keygen', 'configure', 'stage', 'package'])
    parser.add_argument('--key', type=Path)
    parser.add_argument('--public-key', help='Base64 SPKI P-256 public key (configure only)')
    parser.add_argument('--repository', help='Public Releases repository, owner/name')
    parser.add_argument('--channel', choices=['stable', 'preview'], default='stable')
    parser.add_argument('--version')
    parser.add_argument('--tag', help='Release tag; defaults to v<version>')
    parser.add_argument('--url', help='Legacy HTTPS package URL instead of --repository')
    parser.add_argument('--source', type=Path, default=ROOT / 'dist/windows/CyUrium')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/releases')
    args = parser.parse_args()
    if args.version and not re.fullmatch(VERSION, args.version):
        parser.error('Use a numeric major.minor.patch version without leading zeros')
    if args.repository and (not re.fullmatch(REPOSITORY, args.repository) or args.repository.split('/')[1] in ('.', '..')):
        parser.error('Use a repository in owner/name form')
    key = None
    if args.action == 'stage':
        lock=json.loads((ROOT/'engine/runtime.lock.json').read_text(encoding='utf-8'))
        try:print(stage_package(args.source,args.output,lock['sha512']))
        except (OSError,ValueError,RuntimeError) as error:parser.error(str(error))
        return
    if args.action == 'keygen':
        if not args.key:
            parser.error('--key is required')
        if args.key.resolve().is_relative_to(ROOT):
            parser.error('Keep the private signing key outside the source tree')
        if args.key.exists():
            parser.error('The key file already exists')
        key = ec.generate_private_key(ec.SECP256R1())
        args.key.parent.mkdir(parents=True, exist_ok=True)
        with args.key.open('xb') as stream:
            stream.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        print('public_key_spki:', public_key_text(key.public_key()))
        return
    if args.key:
        if args.key.resolve().is_relative_to(ROOT):
            parser.error('Keep the private signing key outside the source tree')
        key = serialization.load_pem_private_key(args.key.read_bytes(), password=None)
        if not isinstance(key, ec.EllipticCurvePrivateKey) or key.curve.name != 'secp256r1':
            parser.error('A P-256 signing key is required')
    if args.action == 'configure':
        if not args.repository:
            parser.error('--repository is required')
        if bool(key) == bool(args.public_key):
            parser.error('Provide exactly one of --key or --public-key')
        try:
            public = key.public_key() if key else serialization.load_der_public_key(base64.b64decode(args.public_key, validate=True))
            if not isinstance(public, ec.EllipticCurvePublicKey) or public.curve.name != 'secp256r1':
                raise ValueError('Not a P-256 public key')
        except ValueError as error:
            parser.error(str(error))
        config = dict(provider='github', repository=args.repository, channel=args.channel, manifest_asset=MANIFEST,
                      public_key_spki=public_key_text(public), platform=PLATFORM, auto_check=True)
        # Validate all source replacements before writing any of them.
        replacements = {}
        if args.version:
            for path, pattern, replacement in (
                (ROOT / 'CMakeLists.txt', r'project\(CyUrium VERSION [0-9.]+ LANGUAGES C CXX\)', f'project(CyUrium VERSION {args.version} LANGUAGES C CXX)'),
                (ROOT / 'engine/host/application.ini', r'(?m)^Version=[0-9.]+$', 'Version=' + args.version),
            ):
                text, count = re.subn(pattern, replacement, path.read_text(encoding='utf-8'))
                if count != 1:
                    parser.error('Could not update version in ' + str(path))
                replacements[path] = text
            app = ROOT / 'engine/host/application.ini'
            replacements[app] = re.sub(r'(?m)^BuildID=\d+$', 'BuildID=' + datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S'), replacements[app])
        for path, text in replacements.items():
            path.write_text(text, encoding='utf-8')
        (ROOT / 'assets/update-channel.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
        print('Configured:', args.repository, args.channel)
        return
    if not key or not args.version:
        parser.error('--key and --version are required for packaging')
    if bool(args.repository) == bool(args.url):
        parser.error('Provide exactly one of --repository or HTTPS --url')
    tag = args.tag or 'v' + args.version
    if tag not in (args.version, 'v' + args.version):
        parser.error('The release tag must match --version (optional v prefix)')
    name = f'CyUrium-{args.version}-{PLATFORM}.zip'
    url = f'https://github.com/{args.repository}/releases/download/{tag}/{name}' if args.repository else args.url
    from urllib.parse import urlsplit
    parsed = urlsplit(url)
    if parsed.scheme != 'https' or not parsed.netloc or parsed.username or parsed.password or parsed.fragment:
        parser.error('A valid HTTPS package URL without credentials is required')
    app = configparser.ConfigParser()
    app.read(args.source / 'engine/app/application.ini', encoding='utf-8')
    if app.get('App', 'Version', fallback='') != args.version:
        parser.error('The built application.ini version must match --version; configure and build first')
    if args.repository:
        config = json.loads((args.source / 'assets/update-channel.json').read_text(encoding='utf-8'))
        if config.get('repository') != args.repository or config.get('public_key_spki') != public_key_text(key.public_key()) or config.get('manifest_asset') != MANIFEST or config.get('provider') != 'github':
            parser.error('The built package must contain this GitHub repository and signing public key; configure and rebuild')
    if args.output.resolve().is_relative_to(args.source.resolve()):
        parser.error('The output directory must be outside the package source')
    lock=json.loads((ROOT/'engine/runtime.lock.json').read_text(encoding='utf-8'))
    try:
        provenance=verify_runtime(args.source/'engine',lock['sha512'])
    except (OSError,ValueError,RuntimeError) as error:parser.error(str(error))
    files, sources = {}, []
    for file in sorted(args.source.rglob('*')):
        relative = file.relative_to(args.source).as_posix()
        if file.is_symlink() or any(part.lower() in ('profile', 'profiles', '.runtime') for part in file.relative_to(args.source).parts):
            parser.error('Package contains a profile or link: ' + relative)
        if not file.is_file() or relative == 'package-files.json':
            continue
        files[relative] = file_hash(file)
        if relative.startswith('engine/') and relative!='engine/runtime-files.json' and files[relative]!=provenance['files'].get(relative.removeprefix('engine/')):
            parser.error('Runtime changed while preparing package: '+relative)
        sources.append((file, relative))
    if not all(name in files for name in ('CyUrium.exe', 'engine/app/application.ini', 'assets/branding/urium-logo.rgba', 'LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md')):
        parser.error('Incomplete browser package (application or licence notices missing)')
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / name
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for file, relative in sources:
            digest=hashlib.sha256()
            with file.open('rb') as stream,package.open(relative,'w') as target:
                for chunk in iter(lambda:stream.read(1048576),b''):
                    digest.update(chunk);target.write(chunk)
            if digest.hexdigest()!=files[relative]:
                parser.error('Package source changed during ZIP creation: '+relative)
        package.writestr('package-files.json', json.dumps(files, separators=(',', ':')))
    payload = json.dumps(dict(version=args.version, platform=PLATFORM, url=url, sha256=file_hash(archive), size=archive.stat().st_size), separators=(',', ':')).encode()
    r, s = utils.decode_dss_signature(key.sign(payload, ec.ECDSA(hashes.SHA256())))
    signature = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')
    envelope = json.dumps(dict(payload=base64.b64encode(payload).decode(), signature=base64.b64encode(signature).decode()), indent=2) + '\n'
    manifest = args.output / MANIFEST
    manifest.write_text(envelope, encoding='utf-8')
    if args.url:  # Existing generic HTTPS deployments keep their manifest name.
        (args.output / 'latest.json').write_text(envelope, encoding='utf-8')
    (args.output / 'SHA256SUMS').write_text('\n'.join(f'{file_hash(file)}  {file.name}' for file in (archive, manifest)) + '\n', encoding='utf-8')
    print(archive)
    print(manifest)
    print('public_key_spki:', public_key_text(key.public_key()))


if __name__ == '__main__':
    main()
