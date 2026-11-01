# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise actual Windows update helper with tiny local executable fixtures."""
from pathlib import Path
import hashlib,json,shutil,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
def main():
    root=ROOT/'.runtime/qa/updater'/str(time.time_ns());root.mkdir(parents=True)
    helper=root/'helper.exe';shutil.copy2(ROOT/'dist/windows/CyUrium/CyUrium.exe',helper)
    fixture=ROOT/'build/win32-release/cyurium_update_fixture.exe'
    results=[]
    for kind in ('success','rollback','tampered'):
        case=root/kind;install=case/'app';profile=case/'profile';stage=profile/'gecko-appdata/updates/payload-test'
        for folder in (install,stage):
            (folder/'engine/app').mkdir(parents=True);(folder/'assets/branding').mkdir(parents=True)
            shutil.copy2(fixture,folder/'CyUrium.exe');(folder/'engine/app/application.ini').write_text('[App]\nName=CyUriumGecko\n')
            (folder/'assets/branding/urium-logo.rgba').write_bytes(b'fixture')
        (install/'version.txt').write_text('old');(stage/'version.txt').write_text('new')
        if kind=='rollback':(stage/'force-fail').write_text('test failure')
        files={p.relative_to(stage).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in stage.rglob('*') if p.is_file()}
        (stage/'package-files.json').write_text(json.dumps(files))
        if kind=='tampered':(stage/'version.txt').write_text('tampered')
        p=subprocess.run([str(helper),'--apply-update',str(stage),str(install),str(profile),'4294967294',str(time.time_ns())],cwd=root,timeout=90)
        report=json.loads((profile/'update-result.json').read_text())
        assert report['success']==(kind=='success'),(kind,p.returncode,report)
        assert (install/'version.txt').read_text()==('new' if kind=='success' else 'old')
        if kind=='rollback':
            for _ in range(20):
                if (profile/'old-restarted.txt').exists():break
                time.sleep(.1)
            assert (profile/'old-restarted.txt').exists()
        results.append({'case':kind,'passed':True,'result':report})
    (ROOT/'.runtime/qa/updater/report.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
if __name__=='__main__':main()
