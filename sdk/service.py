# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Owned CyToolsCore runtime: HTTP/MCP jobs -> private pipes -> native UI thread."""
import hmac
import json
import secrets
import sys
import threading
import time
from pathlib import Path
from cytools_core import Runtime, load_manifest, CyToolError
from cytools_core.transports.http import make_server
from connection import save

MAX_FRAME=4*1024*1024


class BrowserRuntime(Runtime):
    # This browser integration has no local file-input/export contract. Generic
    # SDK controls must not silently broaden browser.read into arbitrary file IO.
    def import_file(self,*args,**kwargs):
        raise CyToolError('AccessDenied','Local file import is not exposed by CyUrium')
    def export_job(self,*args,**kwargs):
        raise CyToolError('AccessDenied','Local file export is not exposed by CyUrium')


class NativeBridge:
    def __init__(self):
        self.lock=threading.RLock()
        self.pending={}
        self.sequence=0
        self.stopped=False
        self.commands=[]
        self.internal_client=''

    def send(self, data):
        text=json.dumps(data,ensure_ascii=True,allow_nan=False)+'\n'
        if len(text)>MAX_FRAME: raise CyToolError('InvalidParameters','Native request too large')
        with self.lock:
            sys.stdout.write(text)
            sys.stdout.flush()

    def invoke(self, context, operation, parameters):
        with self.lock:
            if self.stopped: raise CyToolError('Disconnected','Browser disconnected')
            self.sequence+=1
            request=self.sequence
            event=threading.Event()
            result={}
            self.pending[request]=(event,result)
        context.cancellation.check()
        record=context.runtime.store.all('jobs')[context.job_id]
        self.send(dict(type='request',id=request,operation=operation,parameters=parameters,internal=record['clientId']==self.internal_client))
        try:
            # Once dispatched, wait until native work actually stops. Do not release
            # the SDK reservation early when a queued job is cancelled.
            while not event.wait(.05):
                if self.stopped: raise CyToolError('Disconnected','Browser disconnected')
            context.cancellation.check()
            if 'error' in result:
                error=result['error']
                raise CyToolError(error.get('code','NativeError'),error.get('message','Browser operation failed'))
            return result['result']
        finally:
            with self.lock: self.pending.pop(request,None)

    def read(self):
        try:
            for line in iter(lambda:sys.stdin.buffer.readline(MAX_FRAME+1),b''):
                if len(line)>MAX_FRAME: break
                message=json.loads(line)
                if message.get('type')=='shutdown': break
                if message.get('type')=='assistant':
                    with self.lock:self.commands.append(message['command'])
                    continue
                with self.lock:
                    pending=self.pending.get(message.get('id'))
                    if pending:
                        pending[1].update(message)
                        pending[0].set()
        finally:
            self.stopped=True


def run(profile, control, external=False):
    bridge=NativeBridge()
    private=profile/'sdk/private'
    private.mkdir(parents=True,exist_ok=True,mode=0o700)
    runtime=BrowserRuntime(load_manifest(Path(__file__).with_name('CyTool.json')),private/'jobs',event_limit=256)
    runtime.configure_inputs(private/'inputs',max_bytes=1024,fields={})
    permissions=['browser.read']+(['browser.control'] if control else [])
    for operation in runtime.descriptor['operations']:
        name=operation['id']
        if name.startswith('assistant_'):continue
        runtime.register(name,lambda context,parameters,name=name:bridge.invoke(context,name,parameters))
    from assistant import Assistant
    assistant=Assistant(runtime,bridge,private,control)
    bridge.internal_client=assistant.actor.client_id
    server=make_server(runtime) if external else None
    secret=secrets.token_hex(32)
    from http.server import BaseHTTPRequestHandler
    original=server.RequestHandlerClass if server else BaseHTTPRequestHandler
    class Handler(original):
        def do_POST(self):
            if self.path!='/connect': return super().do_POST()
            self.connection.settimeout(5)
            if self.headers.get('Origin') or not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+secret):
                return self.reply(403,{'error':'Local pairing denied'})
            if self.headers.get('Transfer-Encoding') or self.headers.get('Content-Length')!='2':
                return self.reply(400,{'error':'Invalid pairing request'})
            if self.rfile.read(2)!=b'{}': return self.reply(400,{'error':'Invalid pairing request'})
            self.reply(200,runtime.create_client('CyUrium SDK connection',permissions=permissions))
    if server:server.RequestHandlerClass=Handler
    connection=profile/'sdk/connection.bin'
    published=False
    runtime.start()
    reader=threading.Thread(target=bridge.read,daemon=True)
    reader.start()
    if server:
        worker=threading.Thread(target=server.serve_forever,daemon=True)
        worker.start()
    try:
        if external:
            save(connection,dict(url='http://127.0.0.1:'+str(server.server_port)+'/v1',secret=secret))
            published=True
        bridge.send(dict(type='ready',port=server.server_port if server else 0,version='0.10.1',control=control))
        assistant.publish();last=0
        while not bridge.stopped:
            with bridge.lock:commands,bridge.commands=bridge.commands,[]
            for command in commands:assistant.command(command)
            changed=assistant.tick()
            if changed or time.monotonic()-last>.15 and any(c.get('busy') for c in assistant.chats):
                assistant.publish();last=time.monotonic()
            time.sleep(.05)
    finally:
        if published:connection.unlink(missing_ok=True)
        if external:server.shutdown()
        if server:server.server_close()
        assistant.close()
        runtime.close()


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--control',action='store_true')
    parser.add_argument('--external',action='store_true')
    args=parser.parse_args()
    run(args.profile,args.control,args.external)
