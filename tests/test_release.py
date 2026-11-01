# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the maintainer CLI against disposable source and binary fixtures."""
import base64
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

ROOT = Path(__file__).resolve().parents[1]


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cyurium-release-')
        self.root = Path(self.temp.name) / 'source'
        for folder in ('tools', 'assets', 'engine/host'):
            (self.root / folder).mkdir(parents=True)
        shutil.copy2(ROOT / 'tools/release.py', self.root / 'tools/release.py')
        shutil.copy2(ROOT / 'tools/runtime_provenance.py', self.root / 'tools/runtime_provenance.py')
        (self.root / 'engine/runtime.lock.json').write_text(json.dumps({'sha512':'0'*128}),encoding='utf-8')
        (self.root / 'CMakeLists.txt').write_text('project(CyUrium VERSION 0.3.0 LANGUAGES C CXX)\n', encoding='utf-8')
        (self.root / 'engine/host/application.ini').write_text('[App]\nVersion=0.3.0\nBuildID=20261031000000\n', encoding='utf-8')
        self.key = Path(self.temp.name) / 'private.pem'
        self.cli('keygen', '--key', self.key)
        self.public = serialization.load_pem_private_key(self.key.read_bytes(), password=None).public_key()

    def tearDown(self):
        self.temp.cleanup()

    def cli(self, *args, success=True):
        result = subprocess.run([sys.executable, str(self.root / 'tools/release.py'), *map(str, args)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def configure(self):
        self.cli('configure', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.3.1')
        self.assertIn('VERSION 0.3.1', (self.root / 'CMakeLists.txt').read_text(encoding='utf-8'))
        source = self.root / 'dist/windows/CyUrium'
        for folder in ('engine/app', 'assets/branding'):
            (source / folder).mkdir(parents=True)
        shutil.copy2(self.root / 'engine/host/application.ini', source / 'engine/app/application.ini')
        shutil.copy2(self.root / 'assets/update-channel.json', source / 'assets/update-channel.json')
        (source / 'CyUrium.exe').write_bytes(b'Fixture executable, never executed')
        (source / 'assets/branding/urium-logo.rgba').write_bytes(b'Fixture branding')
        (source/'sdk').mkdir();(source/'sdk/service.py').write_text('# SDK packaging fixture',encoding='utf-8')
        (source/'LICENCES').mkdir();(source/'LICENCES/CyToolsCore-MIT.txt').write_text('Fixture notice',encoding='utf-8')
        for name in ('LICENSE','NOTICE','THIRD_PARTY_NOTICES.md'):
            shutil.copy2(ROOT/name,source/name)
        for name in ('firefox.exe','xul.dll','app/omni.ja'):(source/'engine'/name).write_bytes(b'Fixture runtime, never executed')
        shutil.copy2(self.root/'engine/runtime.lock.json',source/'engine/runtime.lock.json')
        files={p.relative_to(source/'engine').as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (source/'engine').rglob('*') if p.is_file()}
        (source/'engine/runtime-files.json').write_text(json.dumps(dict(version=1,archive_sha512='0'*128,files=files)),encoding='utf-8')
        return source

    def test_github_package_signature_hashes_and_repository(self):
        source = self.configure()
        self.cli('package', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.3.1')
        releases = self.root / 'dist/releases'
        envelope = json.loads((releases / 'latest-windows-x64.json').read_text(encoding='utf-8'))
        payload, signature = base64.b64decode(envelope['payload']), base64.b64decode(envelope['signature'])
        der = utils.encode_dss_signature(int.from_bytes(signature[:32], 'big'), int.from_bytes(signature[32:], 'big'))
        self.public.verify(der, payload, ec.ECDSA(hashes.SHA256()))
        data = json.loads(payload)
        archive = releases / 'CyUrium-0.3.1-windows-x64.zip'
        self.assertEqual(data['url'], 'https://github.com/QA/Releases/releases/download/v0.3.1/' + archive.name)
        self.assertEqual(data['sha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
        self.assertEqual(data['size'], archive.stat().st_size)
        with zipfile.ZipFile(archive) as package:
            files = json.loads(package.read('package-files.json'))
            self.assertEqual(set(files), set(package.namelist()) - {'package-files.json'})
            for name, digest in files.items():
                self.assertEqual(hashlib.sha256(package.read(name)).hexdigest(), digest)
            for name in ('LICENSE','NOTICE','THIRD_PARTY_NOTICES.md'):
                self.assertEqual(package.read(name),(ROOT/name).read_bytes())
        self.assertEqual((source / 'CyUrium.exe').read_bytes(), b'Fixture executable, never executed')
        self.assertIn(data['sha256'], (releases / 'SHA256SUMS').read_text(encoding='utf-8'))

    def test_invalid_version_repository_and_profile_are_rejected(self):
        self.cli('configure', '--key', self.key, '--repository', 'QA/../oops', success=False)
        self.cli('configure', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.03.1', success=False)
        source = self.configure()
        self.cli('package', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.3.2', success=False)
        self.cli('package', '--key', self.key, '--repository', 'QA/Other', '--version', '0.3.1', success=False)
        (source/'LICENSE').unlink()
        result=self.cli('package', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.3.1', success=False)
        self.assertIn('licence notices missing',result.stderr)
        shutil.copy2(ROOT/'LICENSE',source/'LICENSE')
        (source / 'profiles').mkdir()
        (source / 'profiles/sensitive.txt').write_text('Never distribute profiles', encoding='utf-8')
        self.cli('package', '--key', self.key, '--repository', 'QA/Releases', '--version', '0.3.1', success=False)

    def test_wrong_key_and_overwriting_signing_key_are_rejected(self):
        self.configure()
        self.cli('keygen', '--key', self.key, success=False)
        self.cli('keygen', '--key', self.root / 'secret.pem', success=False)
        other = Path(self.temp.name) / 'other.pem'
        self.cli('keygen', '--key', other)
        self.cli('package', '--key', other, '--repository', 'QA/Releases', '--version', '0.3.1', success=False)

    def test_foreign_runtime_files_and_modified_files_are_rejected(self):
        source=self.configure()
        foreign=source/'engine/foreign-autoconfig.cfg';foreign.write_text('must never be published',encoding='utf-8')
        self.cli('package','--key',self.key,'--repository','QA/Releases','--version','0.3.1',success=False)
        clean=self.root/'clean-staging'
        self.cli('stage','--source',source,'--output',clean)
        self.assertFalse((clean/'engine/foreign-autoconfig.cfg').exists())
        self.assertTrue(foreign.exists())  # Never alter the live antivirus integration.
        for name in ('LICENSE','NOTICE','THIRD_PARTY_NOTICES.md'):
            self.assertEqual((clean/name).read_bytes(),(source/name).read_bytes())
        self.cli('package','--source',clean,'--key',self.key,'--repository','QA/Releases','--version','0.3.1')
        (clean/'engine/app/application.ini').write_text('[App]\nVersion=0.3.1\nmodified=true',encoding='utf-8')
        self.cli('package','--source',clean,'--key',self.key,'--repository','QA/Releases','--version','0.3.1',success=False)

if __name__ == '__main__':
    unittest.main()
