# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the native CyMCPSDK conversation panel without provider accounts."""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    for name in ('assistant_integration.py','assistant_tools_integration.py','assistant_migration.py'):
        result=subprocess.call([sys.executable,str(ROOT/'tests'/name),str(ROOT)],cwd=ROOT)
        if result:raise SystemExit(result)
