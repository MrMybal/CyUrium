# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Kill the actual native shell, then verify windows and form restoration."""
from pathlib import Path
from http.server import ThreadingHTTPServer
from test_web import Fixture
from session_codec import read_session,write_session,PREFIX
import subprocess,threading,json,time
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'.runtime/qa/session'/str(time.time_ns());profile=folder/'profile';profile.mkdir(parents=True)
    server=ThreadingHTTPServer(('127.0.0.1',0),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
    exe=ROOT/'dist/windows/CyUrium/CyUrium.exe';session=profile/'session.json';process=None
    try:
        default_profile=folder/'default';default_profile.mkdir()
        initial=subprocess.run([str(exe),'--background-test','--profile',str(default_profile),'--url',f'http://127.0.0.1:{server.server_port}/fixture','--idle-seconds','3'],cwd=ROOT,timeout=30)
        assert initial.returncode==0
        default=read_session(default_profile/'session.json')
        assert all(not (t.get('document') or {}).get('fields') for w in default['windows'] for t in w['tabs'])
        assert (default_profile/'session.json').read_bytes().startswith(PREFIX)
        (profile/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 1\nP 1\n',encoding='utf-8')
        process=subprocess.Popen([str(exe),'--background-test','--profile',str(profile),'--url',f'http://127.0.0.1:{server.server_port}/fixture','--idle-seconds','20'],cwd=ROOT)
        saved=None
        for _ in range(150):
            time.sleep(.1)
            try:
                saved=read_session(session);tab=saved['windows'][0]['tabs'][0]
                if any(f['id']=='state' and f['value'].startswith('form-') for f in (tab.get('document') or {}).get('fields',[])):break
            except (OSError,KeyError,ValueError,TypeError):pass
        else:raise AssertionError('Form checkpoint not written')
        before=next(f['value'] for f in saved['windows'][0]['tabs'][0]['document']['fields'] if f['id']=='state')
        process.kill();process.wait(timeout=10);assert saved['clean'] is False
        # Add a second valid window to the captured snapshot; actual restoration
        # must create a distinct OS viewport as well as the original web tab.
        saved['windows'].append(dict(selected=90,tabs=[dict(id=90,cursor=0,history=[dict(url='cyurium://settings',title='Paramètres')],document=None)],bounds=[-9800,-9800,900,650]))
        write_session(session,saved);metrics=folder/'metrics.json'
        result=subprocess.run([str(exe),'--background-test','--profile',str(profile),'--idle-seconds','4','--metrics',str(metrics)],cwd=ROOT,timeout=30)
        assert result.returncode==0
        restored=read_session(session);assert restored['clean'] and len(restored['windows'])==2
        fields=restored['windows'][0]['tabs'][0]['document']['fields'];assert next(f['value'] for f in fields if f['id']=='state')==before
        numbers=json.loads(metrics.read_text());assert numbers['browser_windows']==2 and numbers['session_recovered'],numbers
        assert before.encode() not in session.read_bytes() and b'http://' not in session.read_bytes()
        report=dict(passed=True,checks=['forms excluded by default','Windows account encryption','actual shell process termination','unclean session marker','two native windows restored','original tab identity','opt-in form value restored into new Gecko document','no plaintext form values or URLs in snapshots','clean exit marker'])
        (folder.parent/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
    finally:
        if process and process.poll() is None:process.kill();process.wait(timeout=10)
        server.shutdown()
if __name__=='__main__':main()
