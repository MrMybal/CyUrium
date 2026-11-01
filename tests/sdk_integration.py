# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
import asyncio
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from client import CyUriumClient
from connection import load
from cytools_core import CyToolError

ROOT=Path(sys.argv[1]);APP=ROOT/'dist/windows/CyUrium/CyUrium.exe'
PROFILE=ROOT/'.runtime/qa/sdk'
CHECKS=[]


class Fixture(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        text=b'''<!doctype html><title>SDK fixture</title><body><h1>Native MCP fixture</h1>
        <input id="query"><input id="password" type="password" value="NEVER_EXPOSE_PASSWORD">
        <textarea id="private">NEVER_EXPOSE_TEXTAREA</textarea><button id="button" onclick="document.querySelector('#result').textContent='Clicked '+document.querySelector('#query').value">Run</button>
        <p id="result">Ready</p><a id="next" href="/next">Next page</a><div style="height:3000px">Scroll</div>'''
        self.send_response(200);self.send_header('Content-Length',str(len(text)));self.end_headers();self.wfile.write(text)


def check(value, label):
    assert value,label
    CHECKS.append(label)


def wait(fn, timeout=30):
    end=time.monotonic()+timeout;last=None
    while time.monotonic()<end:
        try:
            value=fn()
            if value:return value
        except (OSError,ValueError,CyToolError) as error:last=error
        time.sleep(.1)
    raise AssertionError('Timed out: '+str(last))


def invoke(client, operation, parameters=None):
    session=client.call('CreateSession',temporary=True)['sessionId']
    job=client.call('SubmitJob',sessionId=session,operationId=operation,parameters=parameters or {})
    result=wait(lambda:(j if (j:=client.call('GetJob',jobId=job['jobId']))['state'] in ('Completed','Failed','Cancelled') else None))
    client.call('FinishTask',sessionId=session)
    if result['state']!='Completed':raise CyToolError(result['error']['code'],result['error']['message'])
    return result['outputs']


async def real_mcp():
    from mcp import ClientSession,StdioServerParameters
    from mcp.client.stdio import stdio_client
    async with stdio_client(StdioServerParameters(command=str(APP),args=['--mcp','--profile',str(PROFILE)])) as (reader,writer):
        async with ClientSession(reader,writer) as session:
            await session.initialize()
            tools=await session.list_tools()
            check(any(t.name=='op_page_snapshot' for t in tools.tools),'Official MCP discovery through native executable')
            result=await session.call_tool('op_browser_state',{'parameters':{}})
            check(not result.isError,'Official MCP job submission')
            job=result.structuredContent['result'];end=time.monotonic()+10
            while time.monotonic()<end:
                result=await session.call_tool('cy_get_job',{'jobId':job['jobId']})
                if result.structuredContent['result']['state']=='Completed':break
                await asyncio.sleep(.05)
            check(result.structuredContent['result']['state']=='Completed','Official MCP completed native browser job')


def main():
    os.environ['no_proxy']='127.0.0.1,localhost,::1'
    fixture=ThreadingHTTPServer(('127.0.0.1',0),Fixture)
    threading.Thread(target=fixture.serve_forever,daemon=True).start()
    url=f'http://127.0.0.1:{fixture.server_port}/'
    PROFILE.mkdir(parents=True,exist_ok=True)
    # A fresh isolated QA profile, never the user's profile.
    for name in ('settings.txt','session.json','session.json.bak'):(PROFILE/name).unlink(missing_ok=True)
    (PROFILE/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 0\nP 0\nM 1 1\n',encoding='utf-8')
    log=(PROFILE/'native.log').open('wb')
    app=subprocess.Popen([str(APP),'--profile',str(PROFILE),'--background-test','--url',url],stdout=log,stderr=log)
    try:
        client=wait(lambda:CyUriumClient(PROFILE))
        other=CyUriumClient(PROFILE)
        check(client.client_id!=other.client_id,'Per-connection authenticated SDK identities')
        session=client.call('CreateSession',temporary=True)['sessionId']
        job=client.call('SubmitJob',sessionId=session,operationId='browser_state',parameters={})
        try:other.call('GetJob',jobId=job['jobId']);raise AssertionError('Cross-client job visible')
        except CyToolError as error:check(error.code=='AccessDenied','Jobs isolated between SDK clients')
        client.call('FinishTask',sessionId=session,cancelRunning=True)
        denied=client.call('CreateSession',temporary=True)['sessionId']
        try:client.call('ImportFile',sessionId=denied,source=str(ROOT/'README.md'));raise AssertionError('Generic file import enabled')
        except CyToolError as error:check(error.code=='AccessDenied','Generic SDK controls cannot import local files')
        client.call('FinishTask',sessionId=denied)
        state=invoke(client,'browser_state');window=state['windows'][0]['windowId'];tab=state['windows'][0]['selectedTab']
        snapshot=wait(lambda:invoke(client,'page_snapshot',{'tabId':tab,'url':url}))
        check('Native MCP fixture' in snapshot['text'] and 'NEVER_EXPOSE' not in json.dumps(snapshot),'Real Gecko snapshot excludes input/textarea values')
        args={'tabId':tab,'url':url,'documentId':snapshot['documentId']}
        invoke(client,'page_fill',dict(args,selector='#query',text='Café SDK'))
        invoke(client,'page_click',dict(args,selector='#button'))
        updated=invoke(client,'page_snapshot',{'tabId':tab,'url':url})
        check('Clicked Café SDK' in updated['text'],'Native SDK fills and clicks actual Gecko page')
        for params in [dict(args,selector='#password',text='bad'),dict(args,documentId='stale',selector='#button')]:
            try:invoke(client,'page_fill' if 'text' in params else 'page_click',params);raise AssertionError('Restricted action accepted')
            except CyToolError:pass
        check(True,'Sensitive fields and stale document IDs refused')
        second=invoke(client,'tab_detach',{'tabId':tab})['windowId']
        check(second!=window and invoke(client,'page_snapshot',{'tabId':tab,'url':url})['documentId']==snapshot['documentId'],'Detach preserves live Gecko document')
        target=invoke(client,'window_new')['windowId']
        invoke(client,'tab_move',{'tabId':tab,'windowId':target,'before':0})
        check(invoke(client,'page_snapshot',{'tabId':tab,'url':url})['documentId']==snapshot['documentId'],'Cross-window tab move preserves live document')
        try:invoke(client,'tab_navigate',{'tabId':tab,'url':'file:///C:/Windows/win.ini'});raise AssertionError('Local file accepted')
        except CyToolError:check(True,'Privileged and local-file automation excluded')
        discovery=load(PROFILE/'sdk/connection.bin')
        for headers in ({},{'Authorization':'Bearer bad'},{'Authorization':'Bearer '+client.token,'Origin':'https://hostile.example'}):
            request=urllib.request.Request(discovery['url'],data=b'{}',headers=headers)
            try:urllib.request.urlopen(request);raise AssertionError('Unauthenticated/origin accepted')
            except urllib.error.HTTPError as error:check(error.code in (401,403),'HTTP authentication / browser Origin refusal')
        check(discovery['secret'].encode() not in (PROFILE/'sdk/connection.bin').read_bytes(),'Discovery secret protected with OS-account DPAPI')
        asyncio.run(real_mcp())
    finally:
        app.terminate();app.wait(timeout=20);log.close();fixture.shutdown()
    # Native parent crash must stop its owned SDK service, even if stale discovery remains.
    try:client.call('DescribeTool');raise AssertionError('SDK outlived browser')
    except (OSError,urllib.error.URLError):check(True,'Owned SDK process stops with browser')
    (PROFILE/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 0\nP 0\nM 1 0\n',encoding='utf-8')
    with (PROFILE/'readonly.log').open('wb') as readonly_log:
        readonly=subprocess.Popen([str(APP),'--profile',str(PROFILE),'--background-test'],stdout=readonly_log,stderr=readonly_log)
        try:
            limited=wait(lambda:CyUriumClient(PROFILE))
            check(bool(invoke(limited,'browser_state')['windows']),'Read-only client can inspect browser state')
            sid=limited.call('CreateSession',temporary=True)['sessionId']
            try:
                limited.call('SubmitJob',sessionId=sid,operationId='tab_open',parameters={'windowId':1})
                raise AssertionError('Read-only SDK allowed mutation')
            except CyToolError as error:check(error.code=='AccessDenied','Read-only SDK rejects browser mutations before dispatch')
            try:
                limited.call('SubmitJob',sessionId=sid,operationId='assistant_probe',parameters={'connector':'ollama'})
                raise AssertionError('External SDK allowed inference-provider control')
            except CyToolError as error:check(error.code=='AccessDenied','External SDK cannot use the native assistant identity')
            limited.call('FinishTask',sessionId=sid)
        finally:readonly.terminate();readonly.wait(timeout=20)
    report={'passed':True,'sdk':'CyToolsCore 0.10.1','mcp':'1.30.0','checks':CHECKS}
    output=ROOT/'docs/validation/sdk.json';output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
