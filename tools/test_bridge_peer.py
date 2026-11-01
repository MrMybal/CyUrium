# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real Gecko must reject an unauthenticated or forged native server."""
from pathlib import Path
import ctypes,json,os,secrets,socket,subprocess,time
from ctypes import wintypes
from test_services import ROOT

def main():
    results=[]
    for kind in ('preauth-command','wrong-server-proof'):
        profile=ROOT/'.runtime/qa/bridge'/str(time.time_ns());profile.mkdir(parents=True)
        for folder in ('gecko','appdata','localdata'):(profile/folder).mkdir()
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(20)
        secret=secrets.token_hex(32);identifier=secrets.token_hex(16);env=os.environ.copy()
        env.update(MOZ_APP_DATA=str(profile/'appdata'),MOZ_LOCAL_APP_DATA=str(profile/'localdata'),MOZ_NO_REMOTE='1',CYURIUM_GECKO_TOKEN=secret,CYURIUM_GECKO_WINDOW_ID=identifier,CYURIUM_GECKO_PORT=str(listener.getsockname()[1]),CYURIUM_GECKO_TEST='1')
        engine=ROOT/'dist/windows/CyUrium/engine';process=None;peer=None
        with (profile/'runtime.log').open('wb') as log:
            try:
                process=subprocess.Popen([str(engine/'firefox.exe'),'-app',str(engine/'app/application.ini'),'-wait-for-browser','-no-remote','-profile',str(profile/'gecko'),'-chrome','chrome://cyurium/content/manager.xhtml'],cwd=engine,env=env,stdout=log,stderr=log)
                peer,_=listener.accept();peer.settimeout(10);reader=peer.makefile('rb')
                titles=[];user=ctypes.WinDLL('user32',use_last_error=True)
                callback_type=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
                @callback_type
                def collect(window,_):
                    pid=wintypes.DWORD();user.GetWindowThreadProcessId(window,ctypes.byref(pid))
                    if pid.value==process.pid:
                        text=ctypes.create_unicode_buffer(2048);user.GetWindowTextW(window,text,len(text));titles.append(text.value)
                    return True
                user.EnumWindows(collect,0)
                assert any(identifier in title for title in titles),titles
                assert all(secret not in title for title in titles),titles
                if kind=='wrong-server-proof':
                    peer.sendall((json.dumps(dict(type='challenge',protocol=1,nonce=secrets.token_hex(32)))+'\n').encode())
                    hello=json.loads(reader.readline());assert hello['type']=='hello' and 'token' not in hello and secret not in json.dumps(hello)
                    peer.sendall((json.dumps(dict(type='authenticated',protocol=1,proof='0'*64))+'\n').encode())
                else:peer.sendall(b'{"type":"create","tab":999}\n')
                remaining=reader.read();assert b'tab-ready' not in remaining and b'service-state' not in remaining,remaining
                # As in the native shell's job ownership, the parent always
                # reaps an unauthenticated engine after the channel is refused.
                # Gecko's early startup shutdown can await services not started.
                try:process.wait(timeout=1)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
                assert secret.encode() not in (profile/'runtime.log').read_bytes()
                results.append(dict(case=kind,passed=True,window_titles_contain_secret=False))
            finally:
                listener.close()
                if peer:peer.close()
                if process and process.poll() is None:process.kill();process.wait(timeout=10)
    report=ROOT/'.runtime/qa/bridge/report.json';report.write_text(json.dumps(dict(passed=True,cases=results),indent=2),encoding='utf-8');print(report.read_text(encoding='utf-8'))

if __name__=='__main__':main()
