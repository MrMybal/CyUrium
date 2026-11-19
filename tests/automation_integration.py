# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the native CLI/MCP against a real, isolated browser instance."""
from pathlib import Path
import json
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT=Path(__file__).resolve().parents[1]
PACKAGE=ROOT/'dist/windows/CyUrium'
CLI=PACKAGE/'CyUriumCLI.exe'
PROFILE=ROOT/'.runtime/qa/cymcp'/str(time.time_ns())


def cli(*args, ok=True, client='terminal'):
    result=subprocess.run([str(CLI),'--profile',str(PROFILE),'--client',client,*args],capture_output=True,text=True,encoding='utf-8',timeout=20)
    if ok and result.returncode:
        raise AssertionError(result.stderr)
    if not ok:
        assert result.returncode != 0
        assert not result.stdout.strip(), 'Failure polluted JSON stdout'
        return result
    return json.loads(result.stdout)


def main():
    help_result=subprocess.run([str(CLI),'--help'],capture_output=True,text=True,encoding='utf-8',timeout=10)
    assert help_result.returncode==0 and 'Usage:' in help_result.stdout and not help_result.stderr
    PROFILE.mkdir(parents=True)
    (PROFILE/'settings.txt').write_text('CYURIUM 1\n1 1 1\nS 1 1\nP 0\nM 1 1\n',encoding='utf-8')
    with (PROFILE/'app.log').open('wb') as log:
        app=subprocess.Popen([str(PACKAGE/'CyUrium.exe'),'--background-test','--profile',str(PROFILE),'--idle-seconds','120'],stdout=log,stderr=log,cwd=ROOT)
        try:
            end=time.monotonic()+25
            while not (PROFILE/'sdk/connection.bin').exists():
                if app.poll() is not None:raise AssertionError((PROFILE/'app.log').read_text(errors='replace'))
                if time.monotonic()>end:raise AssertionError('Native SDK did not publish discovery')
                time.sleep(.05)
            listing=cli('list');assert listing['sdkVersion']=='0.7.0'
            names=[op['name'] for op in listing['operations']]
            assert all(name in names for name in ('browser_state','bookmarks_add','jobs_submit','resources_read'))
            schema=cli('describe','bookmarks_add');assert schema['inputSchema']['required']==['title','url']
            state=cli('invoke','browser_state');assert state['windows']
            cli('invoke','bookmarks_add','--args',json.dumps({'title':'SDK fixture bookmark','url':'https://example.org/sdk-fixture'}))
            bookmarks=cli('invoke','bookmarks_list');assert any(b['title']=='SDK fixture bookmark' for b in bookmarks['items'])
            assert 'SDK fixture bookmark' in (PROFILE/'settings.txt').read_text(encoding='utf-8')
            cli('invoke','bookmarks_add','--args','{"title":"Blocked","url":"file:///private"}',ok=False)
            cli('invoke','settings_set','--args','{"sdk_enabled":true}',ok=False)
            job=cli('jobs','submit','browser_state')
            for _ in range(50):
                result=cli('jobs','get',job['id'])
                if result['state']=='Completed':break
                time.sleep(.05)
            assert result['result']['windows']
            denied=cli('jobs','get',job['id'],ok=False,client='isolated-peer');assert denied.returncode==4,denied.stderr
            with ThreadPoolExecutor(max_workers=6) as executor:
                parallel=list(executor.map(lambda _:cli('jobs','submit','browser_state',client='parallel-fixture'),range(6)))
            owned=cli('jobs','list',client='parallel-fixture')['items'];assert {j['id'] for j in owned}=={j['id'] for j in parallel}
            mcp=subprocess.Popen([str(CLI),'mcp','--profile',str(PROFILE),'--client','mcp-fixture'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
            try:
                def request(value):
                    mcp.stdin.write(json.dumps(value)+'\n');mcp.stdin.flush();return json.loads(mcp.stdout.readline())
                initialized=request({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'CyUrium fixture','version':'1'}}})
                assert 'result' in initialized,initialized
                tools=request({'jsonrpc':'2.0','id':2,'method':'tools/list','params':{}})
                assert any(t['name']=='bookmarks_list' for t in tools['result']['tools'])
                called=request({'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'browser_state','arguments':{}}})
                assert not called['result'].get('isError',False),called
            finally:
                mcp.stdin.close();mcp.wait(timeout=10)
            print(json.dumps({'passed':True,'sdk':listing['sdkVersion'],'checks':['CLI help and clean diagnostics','native CLI discovery/schema','real browser state','bookmark saved through real UI thread','privileged URL refusal','permission escalation refusal','durable job result','authenticated peer result isolation','concurrent first pairing preserves one job owner','MCP stdio discovery and invocation']},indent=2))
        finally:
            app.terminate();app.wait(timeout=15)


if __name__=='__main__':main()
