# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real Gecko update tests against an isolated loopback GitHub Releases fixture.

Production requires HTTPS. The loopback exception needs both Gecko QA mode and
an explicit test configuration, and never changes certificate trust stores.
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit
import base64
import hashlib
import io
import json
import os
import threading
import time
import zipfile
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from test_services import Bridge, ROOT


def main():
    folder = ROOT / '.runtime/qa/update-channel' / str(time.time_ns())
    folder.mkdir(parents=True)
    signing = ec.generate_private_key(ec.SECP256R1())
    public = base64.b64encode(signing.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)).decode()
    package = io.BytesIO()
    files = {'CyUrium.exe': b'QA does not execute this payload', 'engine/app/application.ini': b'[App]\nName=CyUriumGecko', 'assets/branding/urium-logo.rgba': b'qa'}
    with zipfile.ZipFile(package, 'w') as archive:
        for name, data in files.items():
            archive.writestr(name, data)
        archive.writestr('package-files.json', json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in files.items()}))
    archive = package.getvalue()
    unsafe = io.BytesIO()
    with zipfile.ZipFile(unsafe, 'w') as package:
        package.writestr('../escaped-by-test.txt', b'must never escape the staging folder')
    unsafe_archive = unsafe.getvalue()
    state = dict(mode='valid', requests=[])

    def artifact():
        return unsafe_archive if state['mode'] == 'traversal' else archive

    def manifest(version):
        payload = dict(version=version, platform='windows-x64', url=f'{url}/release/v{version}/CyUrium-{version}-windows-x64.zip', sha256=hashlib.sha256(artifact()).hexdigest(), size=len(artifact()))
        if state['mode'] == 'tag-mismatch':
            payload['version'] = '0.3.9'
        if state['mode'] == 'foreign-package':
            payload['url'] = url + '/foreign.zip'
        if state['mode'] == 'invalid-hash':
            payload['sha256'] = 'g' * 64
        payload = json.dumps(payload).encode()
        r, s = utils.decode_dss_signature(signing.sign(payload, ec.ECDSA(hashes.SHA256())))
        signature = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')
        if state['mode'] == 'bad-signature':
            signature = b'\0' * 64
        return dict(payload=base64.b64encode(payload).decode(), signature=base64.b64encode(signature).decode())

    def release(version='0.3.1', **extra):
        name = f'CyUrium-{version}-windows-x64.zip'
        result = dict(tag_name='v' + version, draft=False, prerelease=False, assets=[
            dict(name='latest-windows-x64.json', state='uploaded', size=1024, browser_download_url=f'{url}/release/v{version}/latest-windows-x64.json'),
            dict(name=name, state='uploaded', size=len(artifact()), browser_download_url=f'{url}/release/v{version}/{name}')])
        return {**result, **extra}

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            state['requests'].append(dict(path=self.path, cookie=self.headers.get('Cookie'), accept=self.headers.get('Accept'), api_version=self.headers.get('X-GitHub-Api-Version')))
            path = urlsplit(self.path).path
            if path.startswith('/repos/'):
                if state['mode'] in ('not-found', 'rate-limit'):
                    self.send_response(404 if state['mode'] == 'not-found' else 429)
                    self.send_header('Retry-After', '60')
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                if '?' in self.path:
                    data = [release('9.0.0', draft=True), release('0.3.1'), release('0.3.2', prerelease=True)]
                else:
                    data = release('0.2.9' if state['mode'] == 'older' else '0.3.1', prerelease=state['mode'] == 'stable-prerelease')
                    if state['mode'] == 'missing-asset':
                        data['assets'] = []
                data = json.dumps(data).encode()
            elif path.startswith('/release/'):
                self.send_response(302)
                location = url + path.replace('/release/', '/cdn/')
                if state['mode'] == 'insecure-redirect':
                    location = f'http://localhost:{server.server_port}/forbidden'
                if state['mode'] == 'redirect-loop':
                    location = url + path
                if state['mode'] == 'insecure-second-redirect':
                    location = url + '/redirect/second'
                self.send_header('Location', location)
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            elif path == '/redirect/second':
                self.send_response(302)
                self.send_header('Location', f'http://localhost:{server.server_port}/forbidden')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            elif path.endswith('.json'):
                version = path.split('/')[2].removeprefix('v') if path.startswith('/cdn/') else '0.3.1'
                data = json.dumps(manifest(version)).encode()
            else:
                data = artifact()
                if state['mode'] == 'corrupt':
                    data = archive[:-1] + bytes([archive[-1] ^ 1])
                if state['mode'] == 'oversized':
                    data = archive + b'oversized'
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Content-Type', 'application/json' if path.endswith('.json') or path.startswith('/repos/') else 'application/octet-stream')
            # A release server must not be able to seed cookies in the user's browser.
            self.send_header('Set-Cookie', 'update_fixture=secret; Path=/')
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f'http://127.0.0.1:{server.server_port}'
    results = []
    bridge = None

    def start(channel='stable', provider='github', automatic=False):
        config = dict(provider=provider, repository='QA/Releases', channel=channel, manifest_asset='latest-windows-x64.json', public_key_spki=public, platform='windows-x64', auto_check=automatic, test_api_url=url, manifest_url=url + '/latest.json')
        path = folder / f'channel-{len(results)}.json'
        path.write_text(json.dumps(config), encoding='utf-8')
        os.environ['CYURIUM_UPDATE_TEST_CONFIG'] = str(path)
        return Bridge()

    def event(status):
        bridge.socket.settimeout(45)
        data = bridge.wait(lambda m: m.get('category') == 'update' and m['data']['status'] in (status, 'error'), timeout=45)['data']
        assert data['status'] == status, data
        return data

    def check(status):
        bridge.send(type='update-check')
        return event(status)

    def close():
        nonlocal bridge
        if bridge:
            bridge.close()
            bridge = None

    try:
        bridge = start()
        state['mode'] = 'bad-signature'
        assert 'Signature' in check('error')['message']
        state['mode'] = 'valid'
        assert check('available')['version'] == '0.3.1'
        for mode, error in [('corrupt', 'Empreinte'), ('oversized', 'taille signée'), ('traversal', 'Chemin de paquet invalide')]:
            state['mode'] = mode
            check('available')
            bridge.send(type='update-download')
            assert error in event('error')['message'], bridge.states['update']
            results.append(mode + ' download rejected')
        assert not (bridge.profile / 'appdata/updates/escaped-by-test.txt').exists()
        state['mode'] = 'valid'
        check('available')
        bridge.send(type='update-download')
        staged = event('staged')
        for name, data in files.items():
            assert (Path(staged['stage']) / name).read_bytes() == data
        before = len(state['requests'])
        check('staged')
        assert len(state['requests']) == before
        results.append('staged update retained across checks')
        # Continue exercising checks after removing only this disposable marker.
        (bridge.profile / 'appdata/updates/staged.json').unlink()
        assert any(r['path'].startswith('/cdn/') for r in state['requests'])
        results.extend(['GitHub latest Release discovery', '302 manifest and package redirects', 'invalid signature rejected', 'P-256 signature accepted', 'corrupted package rejected', 'verified package extracted'])
        for mode, error in [('tag-mismatch', 'tag GitHub'), ('foreign-package', 'absent'), ('invalid-hash', 'Manifeste invalide'), ('missing-asset', 'compatible'), ('not-found', 'Aucune Release'), ('stable-prerelease', 'compatible'), ('insecure-redirect', 'redirection'), ('insecure-second-redirect', 'redirection'), ('redirect-loop', 'redirection')]:
            state['mode'] = mode
            assert error in check('error')['message'], bridge.states['update']
            bridge.send(type='update-download')
            assert 'd’abord' in event('error')['message']  # Failed checks clear stale accepted metadata.
            results.append(mode + ' rejected')
        assert not any(r['path'] == '/forbidden' for r in state['requests'])
        state['mode'] = 'older'
        check('current')
        bridge.send(type='update-download')
        assert 'd’abord' in event('error')['message']
        results.append('older version cannot be installed')
        state['mode'] = 'rate-limit'
        assert 'limite' in check('error')['message']
        before = len(state['requests'])
        assert 'suspendue' in check('error')['message']
        assert len(state['requests']) == before
        results.append('GitHub rate limit respected')
        close()
        state['mode'] = 'valid'
        bridge = start(channel='preview')
        assert check('available')['version'] == '0.3.2'
        results.append('preview accepts newest prerelease and excludes drafts')
        close()
        bridge = start(provider='manifest')
        check('available')
        results.append('legacy signed HTTPS manifest provider retained')
        close()
        bridge = start(automatic=True)
        assert event('available')['version'] == '0.3.1'
        saved = json.loads((bridge.profile / 'appdata/updates/last-check.json').read_text(encoding='utf-8'))
        assert time.time() * 1000 - saved['time'] < 60000
        results.append('automatic check and persisted daily timestamp')
        assert not any(r['cookie'] for r in state['requests'])
        assert all(r['accept'] == 'application/vnd.github+json' and r['api_version'] == '2026-03-10' for r in state['requests'] if r['path'].startswith('/repos/'))
        results.append('API headers and anonymous requests without cookies')
        report = dict(passed=True, transport='isolated loopback HTTP fixture; production requires HTTPS', checks=results)
        (folder.parent / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(json.dumps(report, indent=2), flush=True)
    finally:
        close()
        server.shutdown()
        os.environ.pop('CYURIUM_UPDATE_TEST_CONFIG', None)


if __name__ == '__main__':
    main()
