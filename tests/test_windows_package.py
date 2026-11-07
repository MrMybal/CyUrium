# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from package_windows import validate_package


class WindowsPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        for name in ('CyUrium.exe', 'LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md',
                     'sdk/python/python.exe', 'assets/branding/urium.ico', 'LICENCES/Inno-Setup.txt'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'Packaging fixture, never executed')
        app = self.root / 'engine/app/application.ini'
        app.parent.mkdir(parents=True)
        app.write_text('[App]\nVersion=0.3.0\n', encoding='utf-8')

    def tearDown(self):
        self.temp.cleanup()

    def test_version_and_required_notices(self):
        validate_package(self.root, '0.3.0')
        with self.assertRaisesRegex(ValueError, 'version differ'):
            validate_package(self.root, '0.3.1')
        (self.root / 'LICENSE').unlink()
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            validate_package(self.root, '0.3.0')

    def test_profiles_and_private_keys_rejected(self):
        profile = self.root / 'sdk/profile'
        profile.mkdir()
        with self.assertRaisesRegex(ValueError, 'local-state directory'):
            validate_package(self.root, '0.3.0')
        profile.rmdir()
        (self.root / 'signing.pem').write_text('private-key fixture', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'private-key file'):
            validate_package(self.root, '0.3.0')

    def test_public_certificate_bundle_is_allowed(self):
        ca = self.root / 'sdk/python/Lib/site-packages/certifi/cacert.pem'
        ca.parent.mkdir(parents=True)
        ca.write_text('Public CA bundle fixture', encoding='utf-8')
        validate_package(self.root, '0.3.0')


if __name__ == '__main__':
    unittest.main()
