# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real Gecko service integration tests, with isolated profile and loopback fixture."""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os, socket, subprocess, threading, json, time, secrets, sys, hmac, hashlib
ROOT=Path(__file__).resolve().parents[1]
HTML=b'''<!doctype html><title>Services fixture</title><input id=state><script>
document.documentElement.dataset.instance='services';
addEventListener('cyurium-qa',async e=>{try {
 document.querySelector('#state').value=e.detail+':started:'+document.visibilityState;
 if(e.detail==='geo')await new Promise((ok,no)=>navigator.geolocation.getCurrentPosition(ok,no));
 if(e.detail==='microphone')window.qaStream=await navigator.mediaDevices.getUserMedia({audio:true});
 if(e.detail==='download'){let a=document.createElement('a');a.href='/download';a.download='cyurium-service-test.bin';document.body.append(a);a.click();}
 document.querySelector('#state').value=e.detail+':allowed';
}catch(err){document.querySelector('#state').value=e.detail+':'+err.name;}});
</script>'''
PAYLOAD=b'CyUrium Gecko download fixture\n'*65536
class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        data=PAYLOAD if self.path=='/download' else HTML
        self.send_response(200)
        self.send_header('Content-Type','application/octet-stream' if self.path=='/download' else 'text/html')
        if self.path=='/download':self.send_header('Content-Disposition','attachment; filename="cyurium-service-test.bin"')
        self.send_header('Content-Length',str(len(data)));self.end_headers()
        try:self.wfile.write(data)
        except (BrokenPipeError,ConnectionResetError):pass
    def log_message(self,*args):pass
class Bridge:
    def __init__(self,configuration='windows',profile=None):
        self.profile=profile or ROOT/'.runtime/qa/services'/('profile-'+str(time.time_ns()));self.profile.mkdir(parents=True,exist_ok=True)
        (self.profile/'gecko').mkdir(exist_ok=True)
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(25)
        engine=ROOT/f'dist/{configuration}/CyUrium/engine';env=os.environ.copy();token=secrets.token_hex(32)
        env.update(
            MOZ_APP_DATA=str(self.profile/'appdata'),
            MOZ_LOCAL_APP_DATA=str(self.profile/'localdata'),
            MOZ_NO_REMOTE='1',
            CYURIUM_GECKO_PORT=str(listener.getsockname()[1]),
            CYURIUM_GECKO_TOKEN=token,
            CYURIUM_GECKO_TEST='1',
        )
        env['CYURIUM_GECKO_WINDOW_ID']=secrets.token_hex(16)
        env['MOZ_LOG']='geolocation:5'
        for key in ('MOZ_APP_DATA','MOZ_LOCAL_APP_DATA'):Path(env[key]).mkdir(exist_ok=True)
        self.log=open(self.profile/'runtime.log','wb');self.events=[];self.states={}
        self.process=subprocess.Popen([str(engine/'firefox.exe'),'-app',str(engine/'app/application.ini'),'-wait-for-browser','-no-remote','-profile',str(self.profile/'gecko'),'-chrome','chrome://cyurium/content/manager.xhtml'],env=env,cwd=engine,stdout=self.log,stderr=self.log)
        try:self.socket,_=listener.accept()
        finally:listener.close()
        self.reader=self.socket.makefile('rb');self.socket.settimeout(20)
        challenge=secrets.token_hex(32);self.send(type='challenge',protocol=1,nonce=challenge)
        hello=self.wait(lambda m:m.get('type')=='hello')
        transcript='cyurium-bridge-v1\n'+challenge+'\n'+hello['nonce']+'\n'+str(hello['pid'])+'\n'+hello['version']+'\n'+str(int(hello['remote']))+'\n'+str(int(hello['fission']))
        digest=lambda role:hmac.new(token.encode(),(role+'\n'+transcript).encode(),hashlib.sha256).hexdigest()
        assert hmac.compare_digest(hello['proof'],digest('client')) and hello['pid']==self.process.pid
        self.send(type='authenticated',protocol=1,proof=digest('server'))
        try:self.wait(lambda m:m.get('category')=='ready' and m.get('data') is True)
        except BaseException:
            self.close();raise
    def send(self,**message):self.socket.sendall((json.dumps(message)+'\n').encode())
    def wait(self,predicate,timeout=25):
        start=time.monotonic()
        while time.monotonic()-start<timeout:
            raw=self.reader.readline()
            if not raw:raise RuntimeError('Bridge closed')
            m=json.loads(raw);self.events.append(m)
            (self.profile/'events.json').write_text(json.dumps(self.events,indent=2),encoding='utf-8')
            if m.get('type')=='error':raise RuntimeError(m.get('message')+'\n'+m.get('stack',''))
            if m.get('type')=='service-state':
                self.states[m['category']]=m['data']
                if m['category']=='error':raise RuntimeError(m['data'])
            if predicate(m):return m
        raise TimeoutError('Service event timed out')
    def close(self):
        try:self.send(type='shutdown');self.process.wait(timeout=12)
        except Exception:subprocess.run(['taskkill','/PID',str(self.process.pid),'/T','/F'],capture_output=True)
        self.log.close()
        (self.profile/'events.json').write_text(json.dumps(self.events,indent=2),encoding='utf-8')
def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Fixture);threading.Thread(target=server.serve_forever,daemon=True).start()
    bridge=None
    try:
        bridge=Bridge('windows-debug' if '--debug' in sys.argv else 'windows')
        if '--extensions' in sys.argv:
            # Exercise the manual permission flow separately from the packaged
            # default. Its removal marker must not cause a reinstall at startup.
            extension_id='uBlock0@raymondhill.net'
            assert any(a['id']==extension_id and a['active'] for a in bridge.states['extensions'])
            bridge.send(type='extension-action',id=extension_id,action='remove')
            bridge.wait(lambda m:m.get('category')=='extensions' and not any(a['id']==extension_id for a in m['data']))
            fixture=f'http://127.0.0.1:{server.server_port}/fixture'
            bridge.send(type='workspace-layout',windows=[dict(id=77,selected=1,tabs=[1])])
            bridge.send(type='create',tab=1);bridge.wait(lambda m:m.get('type')=='tab-ready')
            bridge.send(type='navigate',tab=1,sequence=1,action='load',url=fixture)
            bridge.wait(lambda m:m.get('type')=='state' and m.get('title')=='Services fixture' and not m.get('loading'))
            configuration='windows-debug' if '--debug' in sys.argv else 'windows'
            lock=json.loads((ROOT/'engine/bundled-extensions.lock.json').read_text(encoding='utf-8'))
            bridge.send(type='extension-install',path=str(ROOT/f'dist/{configuration}/CyUrium/engine/app/extensions'/lock['extensions'][0]['file']))
            p=bridge.wait(lambda m:m.get('category')=='prompts' and m.get('data'))['data'][0]
            bridge.send(type='prompt-answer',id=p['id'],allow=True,remember=False)
            addon=bridge.wait(lambda m:m.get('category')=='extensions' and any(a['active'] for a in m['data']))['data'][0]
            bridge.send(type='service-qa',action='extension-runtime',id=addon['id'])
            runtime=bridge.wait(lambda m:m.get('category')=='extension-runtime')['data']
            assert any(t['url']==fixture and t['windowId']==77 for t in runtime['tabs']),runtime
            bridge.send(type='extension-popup',id=addon['id'],tab=1)
            bridge.send(type='service-qa',action='panels')
            panels=bridge.wait(lambda m:m.get('category')=='panels')['data']
            assert any(p.get('type')=='popup' and p.get('url','').startswith('moz-extension://') for p in panels),panels
            bridge.send(type='service-qa',action='close-panels')
            for action in ('disable','enable','remove'):
                bridge.send(type='extension-action',id=addon['id'],action=action)
                bridge.wait(lambda m:m.get('category')=='extensions' and (not m['data'] if action=='remove' else any(a['active']==(action=='enable') for a in m['data'])))
            report=dict(passed=True,checks=['Mozilla signature','permission confirmation','existing native tabs visible to extension API','native window identity','real extension popup browser','enable','disable','uninstall'],runtime=runtime)
            (ROOT/'.runtime/qa/services/extensions.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True);return
        url=f'http://127.0.0.1:{server.server_port}/fixture'
        bridge.send(type='create',tab=1);bridge.wait(lambda m:m.get('type')=='tab-ready')
        bridge.send(type='active',tab=1,value=True)
        bridge.send(type='service-qa',action='show',tab=1)
        bridge.send(type='navigate',tab=1,sequence=1,action='load',url=url)
        bridge.wait(lambda m:m.get('type')=='state' and m.get('title')=='Services fixture' and not m.get('loading'))
        bridge.send(type='service-qa',action='exercise',tab=1,value='geo')
        bridge.send(type='inspect',tab=1,request=11)
        prompts=bridge.wait(lambda m:m.get('category')=='prompts' and m.get('data'))['data'];p=prompts[0]
        assert p['origin']==url.rsplit('/',1)[0] and p['title']=='Localisation',p
        bridge.send(type='prompt-answer',id=p['id'],allow=False,remember=True)
        rules=bridge.wait(lambda m:m.get('category')=='permissions' and any(p.get('type')=='geo' and p['value']==2 for p in m['data']))['data']
        bridge.send(type='permission-set',origin=p['origin'],permission='geo',value=0)
        bridge.wait(lambda m:m.get('category')=='permissions' and not any(p.get('type')=='geo' for p in m['data']))
        bridge.send(type='service-qa',action='exercise',tab=1,value='microphone')
        prompts=bridge.wait(lambda m:m.get('category')=='prompts' and m.get('data'))['data'];assert prompts[0]['kind']=='media',prompts
        bridge.send(type='prompt-answer',id=prompts[0]['id'],allow=False,remember=True)
        bridge.wait(lambda m:m.get('category')=='permissions' and any(p.get('type')=='microphone' and p['value']==2 for p in m['data']))
        bridge.send(type='permission-set',origin=p['origin'],permission='microphone',value=0)
        bridge.wait(lambda m:m.get('category')=='permissions' and not any(p.get('type')=='microphone' for p in m['data']))
        bridge.send(type='service-qa',action='exercise',tab=1,value='microphone')
        prompt=bridge.wait(lambda m:m.get('category')=='prompts' and m.get('data'))['data'][0]
        bridge.send(type='prompt-answer',id=prompt['id'],allow=True,remember=False)
        bridge.wait(lambda m:m.get('category')=='media' and m.get('data'))
        bridge.send(type='media-stop',tab=1)
        bridge.wait(lambda m:m.get('category')=='media' and m.get('data')==[])
        bridge.send(type='service-qa',action='exercise',tab=1,value='download')
        items=bridge.wait(lambda m:m.get('category')=='downloads' and any(d['succeeded'] for d in m['data']))['data'];item=next(d for d in items if d['succeeded'])
        target=Path(item['path']);assert target.read_bytes()==PAYLOAD
        bridge.send(type='download-action',id=item['id'],action='remove')
        bridge.wait(lambda m:m.get('category')=='downloads' and not any(d['id']==item['id'] for d in m['data']))
        target.unlink() # Remove only the exact fixture download created by this test.
        bridge.send(type='graphics-refresh');gpu=bridge.wait(lambda m:m.get('category')=='graphics')['data']
        assert gpu.get('adapter') and gpu.get('features'),gpu
        report={'passed':True,'checks':['real geolocation prompt','persisted permission denial','permission reset','real microphone prompt','microphone grant with fake device','active capture indicator','native capture revocation','real attachment download','download bytes and list removal','GPU diagnostics'],'graphics':gpu}
        (ROOT/'.runtime/qa/services/report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2),flush=True)
    finally:
        if bridge:bridge.close();print('Profile:',bridge.profile,flush=True)
        server.shutdown()
if __name__=='__main__':main()
