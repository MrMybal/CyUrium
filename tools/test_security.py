# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise actual Gecko site isolation, form privacy, TLS and phishing defenses."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json, threading, time, ssl, ipaddress
from datetime import datetime, timedelta, timezone
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from test_services import Bridge, ROOT

def main():
    folder=ROOT/'.runtime/qa/security'/str(time.time_ns());folder.mkdir(parents=True)
    requests=[]
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            if self.path=='/frame':data=b'<title>Cross-site frame</title>frame'
            elif self.path=='/csp':data=b'<title>CSP fixture</title><input id=state value=csp-blocked><script>document.querySelector("#state").value="CSP-FAILED"</script>'
            else:
                data=('''<!doctype html><title>Security fixture</title><input id=state value=restorable>
<input id=password type=password value=never-password><input id=token value=never-token>
<input id=card autocomplete=cc-number value=never-card><input id=email type=email value=never-email>
<input id=hidden type=hidden value=never-hidden><form autocomplete=off><input id=privateform value=never-form></form>
<iframe id=frame src="http://localhost:PORT/frame"></iframe>
<script>document.querySelector('#frame').onload=()=>{let blocked=false;try{document.querySelector('#frame').contentWindow.document.body.innerText}catch(e){blocked=e.name==='SecurityError'}
document.querySelector('#state').value=(blocked?'isolated':'ISOLATION-FAILED')+':'+typeof window.Services+':'+typeof window.Cc;document.querySelector('#state').dispatchEvent(new Event('input',{bubbles:true}));};</script>'''.replace('PORT',str(server.server_port))).encode()
            self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(data)))
            if self.path=='/csp':self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'none'")
            self.end_headers();self.wfile.write(data)
        def log_message(self,*args):pass
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
    bridge=None;tls=None
    try:
        bridge=Bridge();initial=bridge.states['security']
        assert initial['remote'] and initial['fission'] and initial['sandboxLevel']>0,initial
        assert initial['safeBrowsing']['initialized'] and initial['safeBrowsing']['phishing'] and initial['safeBrowsing']['malware'],initial
        def open_tab(tab,url):
            bridge.send(type='create',tab=tab);bridge.wait(lambda m:m.get('type')=='tab-ready' and m['tab']==tab)
            bridge.send(type='navigate',tab=tab,sequence=1,action='load',url=url)
            return bridge.wait(lambda m:m.get('type')=='state' and m.get('tab')==tab and m.get('url')==url and not m.get('loading') and (m.get('title') or m.get('error')))
        url=f'http://127.0.0.1:{server.server_port}'
        open_tab(1,url+'/fixture')
        default=bridge.wait(lambda m:m.get('type')=='session-state' and m.get('tab')==1 and m['data']['url']==url+'/fixture')['data']
        assert not default['fields'],default
        bridge.send(type='inspect',tab=1,request=11)
        inspected=bridge.wait(lambda m:m.get('type')=='inspection' and m.get('request')==11)['data']
        assert inspected['value']=='isolated:undefined:undefined',inspected
        bridge.send(type='service-qa',action='security');snapshot=bridge.wait(lambda m:m.get('category')=='security')['data']
        tab=snapshot['tabs'][0];children=tab['frames']['children']
        assert tab['remoteSubframes'] and tab['remoteType'].startswith('webIsolated'),tab
        assert children and children[0]['process']!=tab['frames']['process'] and children[0]['process']>0,tab
        bridge.send(type='session-policy',restoreForms=True)
        opted=bridge.wait(lambda m:m.get('type')=='session-state' and m.get('tab')==1 and m['data']['fields'])['data']
        assert {f['id'] for f in opted['fields']}=={'state'},opted
        bridge.send(type='session-policy',restoreForms=False)
        cleared=bridge.wait(lambda m:m.get('type')=='session-state' and m.get('tab')==1 and not m['data']['fields'])
        open_tab(2,url+'/csp');bridge.send(type='inspect',tab=2,request=12)
        csp=bridge.wait(lambda m:m.get('type')=='inspection' and m.get('request')==12)['data'];assert csp['value']=='csp-blocked',csp
        secure=open_tab(3,'https://example.org/');assert secure['security']=='encrypted' and secure['securityOrigin']=='https://example.org',secure
        # Private self-signed certificate, never imported into any trust store.
        key=ec.generate_private_key(ec.SECP256R1());name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'CyUrium untrusted QA')]);now=datetime.now(timezone.utc)
        cert=x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-timedelta(minutes=1)).not_valid_after(now+timedelta(days=1)).add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]),False).sign(key,hashes.SHA256())
        (folder/'cert.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM));(folder/'tls.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
        tls=ThreadingHTTPServer(('127.0.0.1',0),Handler);context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(folder/'cert.pem',folder/'tls.pem');tls.socket=context.wrap_socket(tls.socket,server_side=True);threading.Thread(target=tls.serve_forever,daemon=True).start()
        before=len(requests);certificate=open_tab(4,f'https://127.0.0.1:{tls.server_port}/untrusted-certificate');assert len(requests)==before,'Untrusted HTTPS response reached the fixture'
        bridge.send(type='service-qa',action='security');tls_report=bridge.wait(lambda m:m.get('category')=='security')['data']
        phishing=open_tab(5,'https://itisatrap.org/firefox/its-a-trap.html')
        bridge.send(type='service-qa',action='security');final=bridge.wait(lambda m:m.get('category')=='security')['data']
        phish=next(t for t in final['tabs'] if t['id']==5)
        assert phish['documentURI'].startswith('about:blocked'),phish
        report=dict(passed=True,checks=['mutually authenticated real Gecko bridge','sandbox effective level above zero','Fission enabled','cross-site iframe in separate process','same-origin DOM access denied','no privileged globals in pages','no form capture by default','sensitive fields excluded after opt-in','disabling capture clears fields','CSP inline script blocked','HTTPS identity from Gecko','untrusted certificate response blocked','Mozilla harmless phishing test blocked'],security=snapshot,certificate=dict(state=certificate,diagnostic=next(t for t in tls_report['tabs'] if t['id']==4)),phishing=phishing)
        (folder.parent/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2),flush=True)
    finally:
        if bridge:bridge.close()
        server.shutdown()
        if tls:tls.shutdown()

if __name__=='__main__':main()
