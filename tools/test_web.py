# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise real Gecko HTML, JS, stateful history, native embedding and tab moves."""
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import subprocess,threading,sys,json
ROOT=Path(__file__).resolve().parents[1]
HTML=b"""<!doctype html><meta charset=utf-8><title>Loading</title>
<style>body{font:18px system-ui;margin:60px;background:#fff9f2;color:#2e2114}h1{color:#995100}input,button{font:inherit;padding:12px;border:1px solid #cbb8a0;border-radius:8px}article{max-width:720px}code{background:#ffe3bc;padding:4px;border-radius:4px}</style>
<article><h1>CyUrium + Gecko</h1><p id=js>JavaScript is running in a separate Gecko content process.</p>
<p>A real HTML page, rendered inside the native Dear ImGui browser window.</p><p><input id=state aria-label="Persistent form state"></p>
<p>This form and its document instance survive back navigation and moving the tab between windows.</p><a href="?page=two">Open the second page</a></article>
<script>document.documentElement.dataset.instance=String(Math.random());document.querySelector('#state').value='form-'+Math.random();document.title=location.search?'Gecko JS page two':'Gecko JS verified';</script>"""
class Fixture(BaseHTTPRequestHandler):
    def handle(self):
        try:super().handle()
        except (ConnectionResetError,BrokenPipeError):pass
    def do_GET(self):
        self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(HTML)));self.end_headers();self.wfile.write(HTML)
    def log_message(self,*args):pass
def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
    output=ROOT/'.runtime/qa/web';output.mkdir(parents=True,exist_ok=True)
    configuration='windows-debug' if '--debug' in sys.argv else 'windows'
    report=output/('debug.json' if '--debug' in sys.argv else 'release.json')
    executable=ROOT/f'dist/{configuration}/CyUrium/CyUrium.exe'
    profile=output/('profile-debug' if '--debug' in sys.argv else 'profile-release')
    profile.mkdir(parents=True,exist_ok=True)
    (profile/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 1\nP 1\n',encoding='utf-8')
    result=subprocess.run([str(executable),'--web-test',str(report),'--profile',str(profile),'--url',f'http://127.0.0.1:{server.server_port}/fixture'],cwd=ROOT,timeout=60)
    server.shutdown()
    print('Web test exit:',result.returncode,flush=True)
    if result.returncode:
        log=profile/'gecko-runtime.log'
        if log.exists():print(log.read_text(errors='replace')[-7000:])
        error=ROOT/'cyurium-error.log'
        if error.exists():print(error.read_text(errors='replace')[-1200:])
        raise SystemExit(result.returncode)
    print(report.read_text())
if __name__=='__main__':main()
