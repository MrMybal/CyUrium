# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare a pinned, standalone Mozilla Gecko runtime. Never uses an installed browser."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,urllib.request,zipfile
from runtime_provenance import archive_files,verify_extracted,file_hash
from prepare_extensions import prepare as prepare_extensions
ROOT=Path(__file__).resolve().parents[1]
def extract(seven,archive,destination,pattern,expected):
    result=subprocess.run([seven,'x','-y','-o'+str(destination),str(archive),pattern],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    output=result.stdout.decode('utf-8',errors='replace')
    # Mozilla's optimized omni.ja layout places its ZIP directory at the front.
    # 7-Zip extracts it but reports a header error; CRC/data errors remain fatal.
    complete=all((destination/name).is_file() for name in expected)
    optimized_header=result.returncode==2 and archive.name=='omni.ja' and 'Headers Error' in output and 'Data Error' not in output and 'CRC Failed' not in output
    if not complete or (result.returncode not in (0,1) and not optimized_header):raise RuntimeError('Gecko extraction failed: '+output[-2000:])
def prepare():
    lock=json.loads((ROOT/'engine/runtime.lock.json').read_text(encoding='utf-8'))
    archive=ROOT/'.cache/downloads'/('Gecko-'+lock['version']+'-'+lock['platform']+'.exe')
    legacy=ROOT/'.cache/downloads/Firefox-153.6.0esr-win64.exe'
    if not archive.exists() and legacy.exists() and hashlib.sha512(legacy.read_bytes()).hexdigest()==lock['sha512']:archive=legacy
    archive.parent.mkdir(parents=True,exist_ok=True)
    if not archive.exists():urllib.request.urlretrieve(lock['url'],archive)
    if hashlib.sha512(archive.read_bytes()).hexdigest()!=lock['sha512']:raise RuntimeError('Gecko archive checksum mismatch')
    seven=shutil.which('7z') or shutil.which('7za')
    if not seven:raise RuntimeError('7-Zip is required to extract the official Gecko runtime')
    revision=lock['sha512'][:16]
    source=ROOT/'.cache/gecko-package'/revision/'core'
    if not (source/'xul.dll').exists():extract(seven,archive,source.parent,'core/*',['core/xul.dll','core/firefox.exe','core/omni.ja','core/browser/omni.ja'])
    entries={name.removeprefix('core/'):entry for name,entry in archive_files(seven,archive,'core/*').items() if name.startswith('core/')}
    official=verify_extracted(source,entries)
    resources=ROOT/('.cache/gecko-resources-'+revision)
    if not (resources/'chrome.manifest').exists():extract(seven,source/'browser/omni.ja',resources,'*',['chrome.manifest','chrome/chrome.manifest','defaults/preferences/firefox.js'])
    resource_files=verify_extracted(resources,archive_files(seven,source/'browser/omni.ja'))
    app=ROOT/'.cache/gecko-app';app.mkdir(exist_ok=True)
    shutil.copy2(ROOT/'engine/host/application.ini',app/'application.ini')
    icons=app/'chrome/icons/default';icons.mkdir(parents=True,exist_ok=True)
    for name in ('default.ico','cyurium-content.ico','cyurium-manager.ico','main-window.ico'):
        shutil.copy2(ROOT/'assets/branding/urium.ico',icons/name)
    manifest=(resources/'chrome.manifest').read_text(encoding='utf-8')+'\n'+(ROOT/'engine/host/chrome.manifest').read_text(encoding='utf-8')
    with zipfile.ZipFile(app/'omni.ja','w',zipfile.ZIP_DEFLATED) as z:
        for name in sorted(resource_files):
            if name!='chrome.manifest':z.write(resources/name,name)
        z.writestr('chrome.manifest',manifest)
        z.writestr('defaults/preferences/zz-cyurium.js',(ROOT/'engine/host/prefs.js').read_text(encoding='utf-8'))
        for file in (ROOT/'engine/host/chrome/content').iterdir():
            if file.is_file():z.write(file,'chrome/cyurium/content/'+file.name)
    extension_files=prepare_extensions(app)
    return source,app,official,lock,extension_files
def copy_changed(source,destination):
    destination=Path(destination)
    for file in Path(source).rglob('*'):
        if not file.is_file():continue
        relative=file.relative_to(source)
        # Runtime remains an application dependency; no updater/service is installed.
        if relative.parts[0] in ('uninstall',) or file.name in ('maintenanceservice.exe','maintenanceservice_installer.exe'):continue
        target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists() or target.stat().st_size!=file.stat().st_size or target.stat().st_mtime_ns!=file.stat().st_mtime_ns:shutil.copy2(file,target)
def package(destination):
    source,app,official,lock,extension_files=prepare();destination=Path(destination)
    approved={}
    for name,digest in official.items():
        relative=Path(name)
        if relative.parts[0]=='uninstall' or relative.name in ('maintenanceservice.exe','maintenanceservice_installer.exe'):continue
        target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists() or file_hash(target)!=digest:shutil.copy2(source/relative,target)
        approved[name]=digest
    # Only files generated by this recipe are copied from the application cache.
    generated=['application.ini','omni.ja']+['chrome/icons/default/'+n for n in ('default.ico','cyurium-content.ico','cyurium-manager.ico','main-window.ico')]+extension_files
    for name in generated:
        target=destination/'app'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(app/name,target);approved['app/'+name]=file_hash(target)
    shutil.copy2(ROOT/'engine/runtime.lock.json',destination/'runtime.lock.json')
    shutil.copy2(ROOT/'engine/MPL-2.0.txt',destination/'MPL-2.0.txt')
    license_folder=ROOT/('.cache/gecko-licenses-'+lock['sha512'][:16])
    license_html=license_folder/'chrome/toolkit/content/global/license.html'
    if not license_html.exists():extract(shutil.which('7z') or shutil.which('7za'),source/'omni.ja',license_folder,'chrome/toolkit/content/global/license.html',['chrome/toolkit/content/global/license.html'])
    shutil.copy2(license_html,destination/'ThirdPartyLicenses.html')
    shutil.copytree(ROOT/'engine/host',destination/'host-source',dirs_exist_ok=True)
    shutil.copy2(ROOT/'tools/prepare_gecko.py',destination/'host-source/prepare_gecko.py')
    shutil.copy2(ROOT/'tools/runtime_provenance.py',destination/'host-source/runtime_provenance.py')
    shutil.copy2(ROOT/'tools/prepare_extensions.py',destination/'host-source/prepare_extensions.py')
    shutil.copy2(ROOT/'engine/bundled-extensions.lock.json',destination/'host-source/bundled-extensions.lock.json')
    shutil.copy2(ROOT/'engine/BUNDLED_EXTENSIONS.md',destination/'host-source/BUNDLED_EXTENSIONS.md')
    own=['runtime.lock.json','MPL-2.0.txt','ThirdPartyLicenses.html']+['host-source/'+p.relative_to(ROOT/'engine/host').as_posix() for p in (ROOT/'engine/host').rglob('*') if p.is_file()]+['host-source/prepare_gecko.py','host-source/runtime_provenance.py','host-source/prepare_extensions.py','host-source/bundled-extensions.lock.json','host-source/BUNDLED_EXTENSIONS.md']
    for name in own:approved[name]=file_hash(destination/name)
    for name,digest in approved.items():
        if file_hash(destination/name)!=digest:raise RuntimeError('Runtime changed during assembly: '+name)
    (destination/'runtime-files.json').write_text(json.dumps(dict(version=1,archive_sha512=lock['sha512'],files=approved),indent=2)+'\n',encoding='utf-8')
    foreign=[p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file() and p.relative_to(destination).as_posix() not in set(approved)|{'runtime-files.json'}]
    if foreign:print('Local runtime additions detected; release packaging will refuse these files:',', '.join(foreign[:8]),flush=True)
    print('Gecko ESR prepared:',destination,flush=True)
if __name__=='__main__':
    package(Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'dist/windows/CyUrium/engine')
