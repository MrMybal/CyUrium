# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Real Core jobs + existing provider service; fixtures never contact an account."""
import json
import tempfile
import threading
import time
import unittest
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from assistant import Assistant,BrowserCodex
from service import BrowserRuntime
from cytools_core import load_manifest,CyToolError
from cyue_mcp.providers.core import descriptors


class Provider(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        data=b'{"models":[{"name":"fixture","model":"fixture"}]}'
        self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    def do_POST(self):
        request=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.send_response(200);self.end_headers()
        try:
            for i in range(30 if request['messages'][-1]['content']=='slow' else 2):
                self.wfile.write(b'{"message":{"content":"token "},"done":false}\n');self.wfile.flush();time.sleep(.05)
            self.wfile.write(b'{"done":true,"message":{"content":""}}\n');self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass


class Bridge:
    def send(self,data):pass


class Tests(unittest.TestCase):
    def test_provider_sessions_cancellation_and_tools(self):
        server=ThreadingHTTPServer(('127.0.0.1',0),Provider)
        threading.Thread(target=server.serve_forever,daemon=True).start()
        import service
        with tempfile.TemporaryDirectory() as tmp:
            runtime=BrowserRuntime(load_manifest(Path(service.__file__).with_name('CyTool.json')),Path(tmp)/'jobs')
            for op in runtime.descriptor['operations']:
                if not op['id'].startswith('assistant_'):runtime.register(op['id'],lambda c,p:{'windows':[{'windowId':42}]})
            host=Assistant(runtime,Bridge(),Path(tmp)/'private')
            runtime.start()
            client=host.client
            session=client.call('CreateSession',temporary=True)['sessionId']
            other=client.call('CreateSession',temporary=True)['sessionId']
            def run(op,params,sid=session):
                job=client.call('SubmitJob',sessionId=sid,operationId=op,parameters=params)
                return runtime.wait(host.actor,job['jobId'])
            try:
                config={'connector':'ollama','endpoint':f'http://127.0.0.1:{server.server_port}','model':'fixture'}
                host.command({'action':'configure','settings':dict(config,api_key_env='sk-never-store-a-key-value')})
                self.assertNotIn('sk-never-store-a-key-value',json.dumps(host.settings))
                self.assertTrue(host.error)
                self.assertNotIn('sk-never-store-a-key-value',host.error)
                a=run('assistant_open',config)['outputs']['conversationId']
                b=run('assistant_open',config,other)['outputs']['conversationId']
                bad=run('assistant_send',{'conversationId':a,'text':'cross-session'},other)
                self.assertEqual(bad['state'],'Failed')
                self.assertEqual(run('assistant_send',{'conversationId':a,'text':'hello'})['state'],'Completed')
                slow=client.call('SubmitJob',sessionId=session,operationId='assistant_send',parameters={'conversationId':a,'text':'slow'})
                time.sleep(.15);client.call('CancelJob',jobId=slow['jobId'])
                self.assertEqual(runtime.wait(host.actor,slow['jobId'])['state'],'Cancelled')
                self.assertEqual(run('assistant_send',{'conversationId':b,'text':'independent'},other)['state'],'Completed')
                # Exercise the actual Codex adapter extension against its upstream
                # request/response parser, without making an inference request.
                transport=host.factory('codex',{'transport':'app-server','cwd':tmp})
                self.assertIsInstance(transport,BrowserCodex)
                class Peer:
                    sent=[]
                    def send(self,value):self.sent.append(value)
                    def next_json(self):return {'id':transport.counter,'result':{}}
                peer=Peer();transport.process=peer
                transport.request('initialize',{})
                self.assertTrue(peer.sent[-1]['params']['capabilities']['experimentalApi'])
                transport.request('thread/start',{})
                tools=peer.sent[-1]['params']['dynamicTools']
                self.assertTrue(any(t['name']=='cyurium_browser_state' for t in tools))
                self.assertFalse(any(t['name']=='cyurium_tab_close' for t in tools))
                transport.reject_request({'id':99,'method':'item/tool/call','params':{'tool':'cyurium_browser_state','arguments':{}}})
                self.assertTrue(peer.sent[-1]['result']['success'])
                self.assertIn('42',peer.sent[-1]['result']['contentItems'][0]['text'])
                transport.process=None
                runtime_record=runtime.store.all('jobs')
                self.assertTrue(all(j['clientId']==host.actor.client_id for j in runtime_record.values()))
            finally:
                host.close();runtime.close();server.shutdown();server.server_close()


if __name__=='__main__':unittest.main()
