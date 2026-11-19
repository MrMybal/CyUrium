# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Isolated native application fixtures; Python is a developer test dependency only."""
import json
import subprocess
import sys
import time
import queue
import threading
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from session_codec import read_session,write_session,transform
PACKAGE=ROOT/'dist/windows/CyUrium'
CLI=PACKAGE/'CyUriumCLI.exe'


class Browser:
    def __init__(self,control=True,options=None,extra_args=()):
        self.extra_args=list(extra_args)
        self.profile=ROOT/'.runtime/qa/cymcp'/str(time.time_ns())
        self.profile.mkdir(parents=True)
        (self.profile/'settings.txt').write_text(f'CYURIUM 1\n1 1 1\nS 1 1\nP 0\nM 1 {int(control)}\n',encoding='utf-8')
        if options is not None:
            (self.profile/'sdk').mkdir()
            write_session(self.profile/'sdk/options.bin',options)
        self.app=None
    def start(self):
        self.log=(self.profile/'app.log').open('ab')
        self.app=subprocess.Popen([str(PACKAGE/'CyUrium.exe'),'--background-test','--profile',str(self.profile),'--idle-seconds','180',*self.extra_args],stdout=self.log,stderr=self.log,cwd=ROOT)
        end=time.monotonic()+30
        while time.monotonic()<end:
            if self.app.poll() is not None:raise AssertionError((self.profile/'app.log').read_text(errors='replace'))
            try:
                self.invoke('browser_state');return self
            except (AssertionError,FileNotFoundError):time.sleep(.1)
        raise AssertionError('Native instance startup timeout')
    def stop(self):
        if self.app and self.app.poll() is None:
            self.app.terminate()
            try:self.app.wait(timeout=15)
            except subprocess.TimeoutExpired:self.app.kill();self.app.wait(timeout=15)
        if hasattr(self,'log'):self.log.close()
        self.app=None
    def cli(self,*args,client='terminal',expected=0,env=None):
        result=subprocess.run([str(CLI),'--profile',str(self.profile),'--client',client,*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=25,env=env)
        assert result.returncode==expected,(result.returncode,result.stderr)
        if expected:assert not result.stdout.strip();return json.loads(result.stderr)
        return json.loads(result.stdout)
    def invoke(self,name,args=None,**kwargs):return self.cli('invoke',name,'--args',json.dumps(args or {}),**kwargs)
    def __enter__(self):return self.start()
    def __exit__(self,*_):self.stop()


class Stdio:
    def __init__(self,browser,gui=False):
        executable=PACKAGE/('CyUrium.exe' if gui else 'CyUriumCLI.exe')
        self.process=subprocess.Popen([str(executable),'--mcp' if gui else 'mcp','--profile',str(browser.profile)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
        self.seq=0
        self.lines=queue.Queue()
        threading.Thread(target=lambda:[self.lines.put(line) for line in self.process.stdout],daemon=True).start()
        self.call('initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'Native integration fixture','version':'1'}})
    def call(self,method,params):
        self.seq+=1
        self.process.stdin.write(json.dumps({'jsonrpc':'2.0','id':self.seq,'method':method,'params':params})+'\n');self.process.stdin.flush()
        try:line=self.lines.get(timeout=20)
        except queue.Empty:self.process.kill();raise AssertionError('MCP stdio response timeout')
        assert line,self.process.poll()
        reply=json.loads(line);assert 'result' in reply,reply;return reply['result']
    def tool(self,name,args=None):return self.call('tools/call',{'name':name,'arguments':args or {}})
    def close(self):self.process.stdin.close();self.process.wait(timeout=15)
