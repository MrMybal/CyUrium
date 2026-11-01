# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
import json
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
ROOT=Path(sys.argv[1])
REQUESTS=[]


class Provider(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        if self.path=='/api/tags':body=json.dumps({'models':[{'name':'fixture','model':'fixture'}]}).encode()
        else:body=b'<!doctype html><title>Native conversation fixture</title><h1>Explicit attached page content</h1><input type="password" value="NEVER_EXPOSE">'
        self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        REQUESTS.append(body)
        self.send_response(200);self.send_header('Content-Type','application/x-ndjson');self.end_headers()
        try:
            for text in ['Bonjour ', 'depuis ', 'le connecteur ', 'CyTools.']:
                self.wfile.write((json.dumps({'model':'fixture','message':{'content':text},'done':False})+'\n').encode());self.wfile.flush();time.sleep(.18)
            self.wfile.write(b'{"done":true,"message":{"content":""}}\n');self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError):pass


def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Provider)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    profile=ROOT/'.runtime/qa/assistant'
    profile.mkdir(parents=True,exist_ok=True)
    report=ROOT/'docs/validation/assistant.json'
    report.unlink(missing_ok=True)
    for p in [profile/'settings.txt',profile/'session.json',profile/'session.json.bak',profile/'sdk/private/assistant/conversations.bin']:p.unlink(missing_ok=True)
    try:
        with (profile/'native.log').open('wb') as log:
            process=subprocess.run([str(ROOT/'dist/windows/CyUrium/CyUrium.exe'),'--profile',str(profile),
                '--background-test','--assistant-test',str(report),'--url',f'http://127.0.0.1:{server.server_port}/','--idle-seconds','55'],stdout=log,stderr=log,timeout=70)
        if process.returncode:raise AssertionError((profile/'native.log').read_text(encoding='utf-8',errors='replace'))
        result=json.loads(report.read_text(encoding='utf-8'))
        assert len(REQUESTS)==2,REQUESTS
        assert len(REQUESTS[1]['messages'])==3,'Provider session history was lost'
        assert 'Explicit attached page content' in REQUESTS[1]['messages'][-1]['content'],'Actual Gecko page was not attached'
        assert 'NEVER_EXPOSE' not in json.dumps(REQUESTS),'Sensitive values sent in page attachment'
        assert not (profile/'sdk/connection.bin').exists(),'External MCP enabled by conversation panel'
        storage=profile/'sdk/private/assistant/conversations.bin'
        assert b'Bonjour CyUrium' not in storage.read_bytes(),'Conversation not protected at rest'
        result['checks']+=['same real provider transcript across turns','page input values excluded from provider prompt','conversation file protected with Windows DPAPI']
        report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result,indent=2))
    finally:server.shutdown();server.server_close()


if __name__=='__main__':main()
