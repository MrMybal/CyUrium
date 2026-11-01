# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the native panel and the bundled Cy provider transports, without accounts."""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    python=str(ROOT/'dist/windows/CyUrium/sdk/python/python.exe')
    subprocess.run([python,'-I','-B',str(ROOT/'tests/assistant_protocol.py')],cwd=ROOT,check=True)
    raise SystemExit(subprocess.call([python,'-I','-B',str(ROOT/'tests/assistant_integration.py'),str(ROOT)],cwd=ROOT))
