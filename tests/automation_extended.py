# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import json
import os
import sqlite3
import threading
import time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from automation_support import Browser,Stdio,read_session,write_session

MARKER='PRIVATE_CAPTURE_TAIL'
class Page(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        body=('<!doctype html><title>Large fixture</title><input id=slow oninput="const until=Date.now()+2000;while(Date.now()&lt;until){}"><p>'+('abcé '*6000)+MARKER+'</p>').encode()
        self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)


def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Page);threading.Thread(target=server.serve_forever,daemon=True).start()
    checks=[]
    try:
        url=f'http://127.0.0.1:{server.server_port}/'
        app=Browser(options={'performance':True,'disk_spool':True,'resource_bytes':4096,'disk_quota_bytes':1048576,'chunk_bytes':1024,'result_ttl_ms':12000},extra_args=['--url',url])
        with app:
            url=f'http://127.0.0.1:{server.server_port}/'
            state=app.invoke('browser_state');window=state['windows'][0]['windowId']
            tab=state['windows'][0]['selectedTab']
            last_error=None
            deadline=time.monotonic()+35
            for attempt in range(100):
                try:
                    capture=app.invoke('page_capture',{'tabId':tab,'url':url,'maxChars':65536});break
                except AssertionError as error:
                    last_error=error
                    if time.monotonic()>deadline:raise
                    if attempt%10==0:print('QA waiting for page:',app.invoke('browser_state'),flush=True)
                    time.sleep(.3)
            else:raise AssertionError(f'Gecko page never became ready: {last_error}; {app.invoke("browser_state")}')
            print('QA: page ready, reading resource chunks',flush=True)
            uri=capture['uri'];manifest=app.invoke('resources_read',{'uri':uri})
            first=json.loads(manifest['contents'][0]['text'])['firstUri'];next_uri=first;data=[]
            while next_uri:
                part=app.invoke('resources_read',{'uri':next_uri});data.append(part['contents'][0]['text']);next_uri=part['_meta']['com.cymcp/chunk'].get('nextUri')
            full=json.loads(''.join(data));assert MARKER in full['text'];assert len(data)>20
            assert any((app.profile/'sdk/private/results').rglob('*.bin')) or any(p.is_file() for p in (app.profile/'sdk/private/results').rglob('*'))
            denied=app.invoke('resources_read',{'uri':uri},client='other',expected=5);assert denied['error']['code']=='ResourceNotFound'
            grep=app.invoke('resources_grep',{'text':MARKER});assert grep['matches']
            checks+=['full UTF-8 page recovered through private chunks','disk spool used','cross-client resource denied','native grep without filesystem access']
            job=app.cli('jobs','submit','page_capture','--args',json.dumps({'tabId':tab,'url':url,'maxChars':65536}))
            for _ in range(60):
                result=app.cli('jobs','get',job['id'])
                if result['state']=='Completed':break
                time.sleep(.05)
            assert MARKER in result['result']['text'],result
            cancelled=app.cli('jobs','submit','page_fill','--args',json.dumps({'tabId':tab,'url':url,'documentId':full['documentId'],'selector':'#slow','text':'cancellation fixture'}))
            app.cli('jobs','cancel',cancelled['id'])
            for _ in range(40):
                cancelled_result=app.cli('jobs','get',cancelled['id'])
                if cancelled_result['state'] in ('Cancelled','Completed','Failed'):break
                time.sleep(.05)
            assert cancelled_result['state']=='Cancelled',cancelled_result
            checks+=['cooperative cancellation of a real Gecko operation']
            print('QA: durable page stored, checking GUI --mcp',flush=True)
            mcp=Stdio(app,gui=True)
            try:
                assert mcp.tool('browser_state').get('isError',False) is False
                assert mcp.tool('bookmarks_add',{'title':'MCP real mutation','url':'https://example.org/mcp'}).get('isError',False) is False
                assert any(b['title']=='MCP real mutation' for b in app.invoke('bookmarks_list')['items'])
                assert mcp.tool('settings_set',{'sdk_control':True})['isError']
            finally:mcp.close()
            checks+=['existing GUI --mcp preserved','real MCP mutation visible through CLI','MCP permission escalation refused']
            time.sleep(12.1)
            assert app.invoke('resources_read',{'uri':first},expected=5)['error']['code'] in ('ResourceNotFound','ExpiredResource','UnknownResource')
            perf=(app.profile/'sdk/private/performance.jsonl').read_text()
            assert MARKER not in perf and url not in perf and 'MCP real mutation' not in perf
            assert all(set(json.loads(line)) for line in perf.splitlines())
            checks+=['expired reference fails without replay','performance JSONL excludes fixture contents']
        pending=read_session(app.profile/'sdk/private/jobs-v2'/(job['id']+'.bin'))
        pending.update(id='interrupted-fixture',state='Running',operation='bookmarks_add');pending.pop('result',None)
        write_session(app.profile/'sdk/private/jobs-v2/interrupted-fixture.bin',pending)
        with app:
            result=app.cli('jobs','get',job['id']);assert MARKER in result['result']['text']
            assert app.cli('jobs','get',job['id'],client='other',expected=4)
            assert app.cli('jobs','get','interrupted-fixture')['state']=='Interrupted'
            checks+=['durable full result survives restart and SDK resource expiry']
            # Original legacy format, ownership and original database preservation.
            token='legacy-fixture-credential-for-own-jobs';folder=app.profile/'sdk/private/jobs';folder.mkdir()
            database=folder/'state.sqlite3'
            with sqlite3.connect(database) as db:
                db.execute('CREATE TABLE records(kind TEXT,id TEXT,value TEXT,PRIMARY KEY(kind,id))')
                records=[('clients','old-client',{'clientId':'old-client','tokenHash':hashlib.sha256(token.encode()).hexdigest()}),('jobs','old-job',{'jobId':'old-job','clientId':'old-client','operationId':'browser_state','state':'Completed','outputs':{'legacyResult':42}}),('jobs','other-job',{'jobId':'other-job','clientId':'another-owner','operationId':'browser_state','state':'Completed','outputs':{'private':True}})]
                db.executemany('INSERT INTO records VALUES(?,?,?)',[(k,i,json.dumps(v)) for k,i,v in records])
            original=database.read_bytes();env=dict(os.environ,CYURIUM_LEGACY_QA=token)
            rows=app.cli('--legacy-token-env','CYURIUM_LEGACY_QA','jobs','list',env=env)['items']
            imported=[r for r in rows if r.get('legacyId')=='old-job'];assert len(imported)==1 and not any(r.get('legacyId')=='other-job' for r in rows)
            assert app.cli('jobs','get',imported[0]['id'])['result']['legacyResult']==42
            app.cli('--legacy-token-env','CYURIUM_LEGACY_QA','jobs','list',env=env)
            assert database.read_bytes()==original
            checks+=['legacy SQLite jobs imported once with original token','other legacy owners excluded','original legacy storage unchanged']
        offline=app.cli('--offline','invoke','bookmarks_list');assert any(b['title']=='MCP real mutation' for b in offline['items'])
        checks+=['offline saved bookmarks without launching GUI']
        with Browser(control=False) as readonly:
            assert readonly.invoke('browser_state')['windows']
            assert readonly.invoke('bookmarks_add',{'title':'Denied','url':'https://example.org'},expected=4)
            mcp=Stdio(readonly)
            try:assert mcp.tool('bookmarks_add',{'title':'Denied','url':'https://example.org'})['isError']
            finally:mcp.close()
        checks+=['same read-only host policy through CLI and MCP']
        print(json.dumps({'passed':True,'checks':checks},indent=2))
    finally:server.shutdown();server.server_close()


if __name__=='__main__':main()
