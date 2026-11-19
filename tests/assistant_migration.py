# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Legacy DPAPI transcripts and native checkpoint restoration, without account access."""
import json
import queue
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from automation_support import Browser,CLI,read_session,transform

GENERATIONS=[]
class Provider(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        body=b'{"models":[{"name":"fixture","model":"fixture"}]}'
        self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    def do_POST(self):
        GENERATIONS.append(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
        body=b'{"message":{"content":"Restored answer"},"done":true}\n'
        self.send_response(200);self.send_header('Content-Type','application/x-ndjson');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)


class Agent:
    def __init__(self,profile):
        self.process=subprocess.Popen([str(CLI),'serve','--profile',str(profile)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
        self.lines=queue.Queue();threading.Thread(target=lambda:[self.lines.put(json.loads(line)) for line in self.process.stdout],daemon=True).start()
    def command(self,action,**args):
        self.process.stdin.write(json.dumps({'type':'assistant','command':dict(action=action,**args)})+'\n');self.process.stdin.flush()
    def state(self,predicate=lambda _:True):
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            message=self.lines.get(timeout=15)
            if message.get('type')=='assistant-state' and predicate(message['data']):return message['data']
        raise AssertionError('Conversation state timeout')
    def close(self):self.process.stdin.close();assert self.process.wait(timeout=15)==0,self.process.stderr.read()


def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Provider);threading.Thread(target=server.serve_forever,daemon=True).start()
    profile=Browser().profile;folder=profile/'sdk/private/assistant';folder.mkdir(parents=True)
    cfg={'connector':'ollama','transport':'http-ndjson','model':'fixture','endpoint':f'http://127.0.0.1:{server.server_port}','api_key_env':'','executable':''}
    messages=[{'role':'user' if i%2==0 else 'assistant','text':f'Legacy row {i}'} for i in range(30)]
    messages[2]['text']='Large legacy text '+('é'*50000)+' END_OF_FULL_MESSAGE'
    fixture={'settings':cfg,'conversations':[{'id':'api','settings':cfg,'messages':messages,'title':'Legacy API'},{'id':'cli','settings':dict(cfg,connector='codex',endpoint=''),'messages':[{'role':'user','text':'Old CLI transcript'}],'title':'Legacy CLI'},{'id':'crashed','settings':cfg,'messages':[{'role':'user','text':'Do not replay'}],'title':'Interrupted','busy':True,'partial':'Interrupted partial','pendingContent':'Do not replay'}]}
    (folder/'conversations.bin').write_bytes(transform(json.dumps(fixture).encode(),True))
    try:
        agent=Agent(profile)
        try:
            state=agent.state();assert not GENERATIONS
            assert any(m.get('interrupted') for m in state['conversations'][-1]['messages'])
            agent.command('history',conversationId='api',offset=0);state=agent.state(lambda s:s['conversations'][0].get('historyOffset')==0)
            assert state['conversations'][0]['messages'][0]['text']=='Legacy row 0'
            assert state['conversations'][0]['historyCount']==30
            large=state['conversations'][0]['messages'][2];assert large['nextTextOffset']<=64000
            agent.command('message-read',conversationId='api',index=2,offset=large['nextTextOffset'])
            state=agent.state(lambda s:'END_OF_FULL_MESSAGE' in s['conversations'][0]['messages'][2]['text'])
            agent.command('send',conversationId='api',text='Explicit migration turn')
            agent.state(lambda s:len(s['conversations'][0].get('messages',[]))>0 and not s['conversations'][0].get('busy') and s['conversations'][0].get('historyCount')==32)
            assert 'Legacy row 0' in json.dumps(GENERATIONS[0])
            agent.command('send',conversationId='cli',text='Explicit CLI migration')
            state=agent.state(lambda s:bool(s['conversations'][1].get('error')))
            assert 'Legacy' in state['conversations'][1]['error']
            agent.command('fork',conversationId='cli');agent.state(lambda s:len(s['conversations'])==4)
        finally:agent.close()
        saved=read_session(folder/'conversations.bin');assert len(saved['conversations'][0]['messages'])==32
        assert saved['conversations'][0]['checkpoint']['provider']=='ollama'
        assert saved['conversations'][-1]['legacyContext'][0]['text']=='Old CLI transcript'
        assert len(GENERATIONS)==1
        resumed=Agent(profile)
        try:
            resumed.state();assert len(GENERATIONS)==1
            resumed.command('send',conversationId='api',text='Explicit resumed turn')
            resumed.state(lambda s:s['conversations'][0].get('historyCount')==34 and not s['conversations'][0].get('busy'))
            assert 'Explicit migration turn' in json.dumps(GENERATIONS[1]) and 'Legacy row 0' in json.dumps(GENERATIONS[1])
        finally:resumed.close()
        print(json.dumps({'passed':True,'checks':['raw legacy DPAPI format read','full history pagination','API transcript migrated on explicit send','native checkpoint restored across process restart','legacy CLI transcript preserved with explicit fork','crashed pending turn retained without replay']},indent=2))
    finally:server.shutdown();server.server_close()


if __name__=='__main__':main()
