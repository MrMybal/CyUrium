# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real Gecko: default uBO filters and persistent user choices, no live sites."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import sys
import threading
from test_services import Bridge, ROOT

ADDON = 'uBlock0@raymondhill.net'
MARKER = 'cyurium.extensions.defaults.'+ADDON
HTML = b'''<!doctype html><title>Loading uBlock fixture</title>
<input id=state><p>Normal page content</p>
<script>window.qaControl=false;window.qaAd=false;</script>
<script src="/content.js"></script><script src="/ads/custom_ads.js"></script>
<script>addEventListener('load',()=>{
 document.querySelector('#state').value='control:'+qaControl+';ad:'+qaAd;
 document.title='uBlock fixture done';});</script>'''


class Fixture(BaseHTTPRequestHandler):
    requests = []

    def do_GET(self):
        self.requests.append(self.path)
        body = HTML
        if self.path == '/content.js':
            body = b'window.qaControl=true;'
        elif self.path == '/ads/custom_ads.js':
            body = b'window.qaAd=true;'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html' if body is HTML else 'application/javascript')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


def page(bridge, url, blocked):
    Fixture.requests.clear()
    bridge.send(type='workspace-layout', windows=[dict(id=77, selected=1, tabs=[1])])
    bridge.send(type='create', tab=1)
    bridge.wait(lambda m: m.get('type') == 'tab-ready')
    bridge.send(type='active', tab=1, value=True)
    bridge.send(type='navigate', tab=1, sequence=1, action='load', url=url)
    bridge.wait(lambda m: m.get('type') == 'state' and m.get('title') == 'uBlock fixture done' and not m.get('loading'))
    bridge.send(type='inspect', tab=1, request=51)
    result = bridge.wait(lambda m: m.get('type') == 'inspection' and m.get('request') == 51)['data']
    assert result['value'] == 'control:true;ad:'+('false' if blocked else 'true'), result
    assert '/content.js' in Fixture.requests, Fixture.requests
    assert ('/ads/custom_ads.js' not in Fixture.requests) == blocked, Fixture.requests
    return dict(blocked=blocked, requests=list(Fixture.requests), page=result['value'])


def addon(bridge):
    return next((a for a in bridge.states['extensions'] if a['id'] == ADDON), None)


def action(bridge, value):
    bridge.send(type='extension-action', id=ADDON, action=value)
    bridge.wait(lambda m: m.get('category') == 'extensions' and (
        not any(a['id'] == ADDON for a in m['data']) if value == 'remove' else
        any(a['id'] == ADDON and a['active'] == (value == 'enable') for a in m['data'])))


def main():
    server = ThreadingHTTPServer(('127.0.0.1', 0), Fixture)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    configuration = 'windows-debug' if '--debug' in sys.argv else 'windows'
    bridge = None
    observations = []
    try:
        bridge = Bridge(configuration)
        profile = bridge.profile
        initial = addon(bridge)
        assert initial and initial['active'] and initial['signed'], initial
        assert not bridge.states.get('prompts'), bridge.states.get('prompts')
        url = f'http://127.0.0.1:{server.server_port}/fixture'
        # The first page after a new profile must already be filtered, using
        # the original embedded EasyList rule /ads/custom_ads.js$script.
        observations.append(page(bridge, url, True))
        action(bridge, 'disable')
        bridge.close();bridge = None

        # Simulate an older CyUrium profile containing a disabled copy but no
        # default-install marker. Migration must not re-enable or replace it.
        prefs = profile/'gecko/prefs.js'
        prefs.write_text('\n'.join(line for line in prefs.read_text(encoding='utf-8').splitlines()
                                   if MARKER not in line)+'\n', encoding='utf-8')
        bridge = Bridge(configuration, profile)
        disabled = addon(bridge)
        assert disabled and disabled['disabled'] and not disabled['active'], disabled
        observations.append(page(bridge, url, False))
        action(bridge, 'enable')
        bridge.close();bridge = None

        bridge = Bridge(configuration, profile)
        assert addon(bridge)['active'], addon(bridge)
        observations.append(page(bridge, url, True))
        action(bridge, 'remove')
        bridge.close();bridge = None

        bridge = Bridge(configuration, profile)
        assert addon(bridge) is None, addon(bridge)
        observations.append(page(bridge, url, False))
        report = dict(passed=True, version=initial['version'], addon=ADDON,
                      checks=['packaged Mozilla-signed extension installed without a prompt',
                              'first page filtered with bundled EasyList; no public site needed',
                              'normal script preserved; ad script never reaches fixture server',
                              'existing disabled installation respected during migration',
                              're-enabled extension filters after restart',
                              'uninstall preserved after restart'], observations=observations)
        target = ROOT/'.runtime/qa/services/adblock.json'
        target.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(report, indent=2), flush=True)
    finally:
        if bridge:
            bridge.close()
        server.shutdown()


if __name__ == '__main__':
    main()
