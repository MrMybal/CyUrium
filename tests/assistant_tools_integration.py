# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real native panel + real remote browser; only the model is an HTTP fixture."""
import hashlib
import json
import os
import subprocess
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from automation_support import Browser,PACKAGE,ROOT,read_session,write_session

REQUESTS=[]
class Provider(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        body=(json.dumps({'models':[{'name':'fixture','model':'fixture'}]}) if self.path=='/api/tags' else '<!doctype html><title>Attached page</title><h1>Real attached text</h1><input type="password" value="NO_PASSWORD_LEAK">').encode()
        self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])));REQUESTS.append(body)
        self.send_response(200);self.send_header('Content-Type','application/x-ndjson');self.end_headers()
        try:
            content=json.dumps(body['messages'][-1].get('content',''))
            if 'Stream until cancelled' in content:
                for _ in range(200):self.frame('cancel-visible ');time.sleep(.06)
            elif 'Explain the attached page' in content:
                self.frame('Attached page received. ');time.sleep(.2);self.frame('Answer complete.');self.terminal()
            else:
                step=self.server.steps;self.server.steps+=1
                remote=self.server.remote
                resource='remote_peer_resources_read'
                calls=[('cy_tools_search',{'query':'bookmarks_add'}),('cy_tools_describe',{'name':'bookmarks_add'}),('cy_tools_call',{'name':'bookmarks_add','arguments':{'title':'Panel tool bookmark','url':'https://example.org/panel'}}),('cy_tools_call',{'name':self.server.remote_add,'arguments':{'title':'Remote panel bookmark','url':'https://example.org/remote'}}),('cy_tools_call',{'name':self.server.remote_error,'arguments':{'tabId':99999999,'url':'https://example.org'}}),('cy_tools_call',{'name':resource,'arguments':{'uri':'cyurium://browser/state'}}),('cy_tools_call',{'name':resource,'arguments':{'uri':'cyurium://browser/state'}}),('cy_tools_call',{'name':resource,'arguments':{'uri':'cyurium://browser/state'}})]
                if step==7:
                    state=remote.invoke('browser_state');remote.invoke('tab_open',{'windowId':state['windows'][0]['windowId'],'url':f'http://127.0.0.1:{self.server.server_port}/changed'})
                    time.sleep(.5)
                if step<len(calls):
                    name,args=calls[step];self.wfile.write((json.dumps({'message':{'content':'','tool_calls':[{'function':{'name':name,'arguments':args}}]},'done':False})+'\n').encode());self.wfile.flush();self.terminal()
                else:self.frame('The native and remote tools ran. ');time.sleep(.25);self.frame('Response complete.');self.terminal()
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass
    def frame(self,text):self.wfile.write((json.dumps({'model':'fixture','message':{'content':text},'done':False})+'\n').encode());self.wfile.flush()
    def terminal(self):self.wfile.write(b'{"done":true,"message":{"content":""}}\n');self.wfile.flush()


def main():
    with Browser(options={'cache_ttl_ms':60000}) as remote:
        root=read_session(remote.profile/'sdk/connection.bin')
        request=urllib.request.Request(root['url'],json.dumps({'method':'pair','key':'remote-native-integration-fixture-key-123456789'}).encode(),{'Authorization':'Bearer '+root['secret'],'Content-Type':'application/json'})
        with urllib.request.urlopen(request) as response:connection=json.load(response)
        remote_url=connection['url'][:-3]+'/mcp'
        server=ThreadingHTTPServer(('127.0.0.1',0),Provider);server.steps=0;server.remote=remote
        server.remote_add='remote_peer_'+hashlib.sha256(b'bookmarks_add').hexdigest()[:12]
        server.remote_error='remote_peer_'+hashlib.sha256(b'tab_navigate').hexdigest()[:12]
        threading.Thread(target=server.serve_forever,daemon=True).start()
        host=Browser(options={'context_bytes':32768,'context_recent_turns':1,'cache_ttl_ms':60000,'performance':True,'remote_servers':[{'id':'peer','enabled':True,'url':remote_url,'token_env':'CYURIUM_REMOTE_QA','allowed_tools':['bookmarks_add','tab_navigate'],'allowed_resources':['cyurium://browser/state']}]})
        # Integrated chat can be enabled independently of external MCP.
        (host.profile/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 1\nP 0\nM 0 1\n',encoding='utf-8')
        report=host.profile/'assistant-sdk.json'
        try:
            with (host.profile/'native.log').open('wb') as log:
                process=subprocess.run([str(PACKAGE/'CyUrium.exe'),'--profile',str(host.profile),'--background-test','--assistant-sdk-test',str(report),'--url',f'http://127.0.0.1:{server.server_port}/','--idle-seconds','85'],stdout=log,stderr=log,timeout=95,env=dict(os.environ,CYURIUM_REMOTE_QA=connection['secret']))
            assert process.returncode==0,(host.profile/'native.log').read_text(errors='replace')
            result=json.loads(report.read_text())
            assert 'Panel tool bookmark' in (host.profile/'settings.txt').read_text()
            assert any(b['title']=='Remote panel bookmark' for b in remote.invoke('bookmarks_list')['items'])
            assert len(REQUESTS)>=11,len(REQUESTS)
            assert {tool['function']['name'] for tool in REQUESTS[0]['tools']}=={'cy_tools_search','cy_tools_describe','cy_tools_call','cy_resources_read','cy_resources_grep'}
            saved=read_session(host.profile/'sdk/private/assistant/conversations.bin');chat=saved['conversations'][-1]
            history=json.dumps(chat['checkpoint']['messages'],ensure_ascii=False)
            assert 'bookmarks_add' in history and 'RemoteToolError' in history,history[-6000:]
            assert 'Panel tool bookmark' in history and 'Remote panel bookmark' in history
            assert any(m.get('interrupted') for m in chat['messages']) and len(chat['messages'])==6
            second=[r for r in REQUESTS if 'Explain the attached page' in json.dumps(r['messages'][-1])][0]
            assert 'Bonjour CyUrium' not in json.dumps(second['messages'])
            assert 'Bonjour CyUrium' in json.dumps(chat['messages'])
            assert 'NO_PASSWORD_LEAK' not in json.dumps(REQUESTS)
            # Cache hit followed by a server change before its sixty-second TTL.
            resources=[]
            for message in REQUESTS[8]['messages']:
                if message['role']=='tool' and 'cyurium://browser/state' in str(message):resources.append(message)
            assert len(resources)>=3,len(resources)
            assert '/changed' not in json.dumps(resources[0]) and '/changed' not in json.dumps(resources[1]) and '/changed' in json.dumps(resources[2])
            performance=(host.profile/'sdk/private/performance.jsonl').read_text()
            assert connection['secret'] not in performance and 'Panel tool bookmark' not in performance and 'Bonjour CyUrium' not in performance
            events=[json.loads(line) for line in performance.splitlines()]
            assert any(event.get('cache_hit') or event.get('cacheHit') for event in events),events[-10:]
            assert not (host.profile/'sdk/connection.bin').exists()
            result['checks']+=['compact search/describe/call path','real native bookmark from chat','real allowlisted remote mutation from chat','remote tool error returned to model','selected remote resources','remote cache hit and notification invalidation before TTL','API context reduced while full history/tool groups retained','cancelled turn persisted','private performance metadata only']
            print(json.dumps(result,indent=2))
        finally:
            (host.profile/'provider-requests.json').write_text(json.dumps(REQUESTS,indent=2),encoding='utf-8')
            server.shutdown();server.server_close()


if __name__=='__main__':main()
