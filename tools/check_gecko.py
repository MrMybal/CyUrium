# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Check the pinned ESR against Mozilla's published version (no installation)."""
from pathlib import Path
import argparse,json,re,urllib.request
ROOT=Path(__file__).resolve().parents[1]
URL='https://product-details.mozilla.org/1.0/firefox_versions.json'

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--metadata',type=Path,help='Offline QA fixture instead of Mozilla HTTPS');args=parser.parse_args()
    lock=json.loads((ROOT/'engine/runtime.lock.json').read_text(encoding='utf-8'))
    if args.metadata:data=json.loads(args.metadata.read_text(encoding='utf-8'))
    else:
        with urllib.request.urlopen(URL,timeout=30) as response:
            raw=response.read(65537)
            if len(raw)>65536:raise RuntimeError('Mozilla version response is too large')
            data=json.loads(raw)
    def version(value):
        if not isinstance(value,str) or not re.fullmatch(r'\d+(?:\.\d+){1,3}(?:esr)?',value):raise RuntimeError('Invalid Mozilla version')
        numbers=tuple(map(int,value.removesuffix('esr').split('.')));return numbers+(0,)*(4-len(numbers))
    current,latest=lock['version'],data['FIREFOX_ESR']
    outdated=version(current)<version(latest)
    print(json.dumps(dict(current=current,mozilla_esr=latest,update_required=outdated,source=URL),indent=2))
    return 1 if outdated else 0

if __name__=='__main__':raise SystemExit(main())
