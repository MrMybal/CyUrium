# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Run the native CLI/MCP integration scenario against a real browser instance."""
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    import sys
    for name in ('automation_integration.py','automation_extended.py'):
        result=subprocess.call([sys.executable,str(ROOT/'tests'/name),*sys.argv[1:]],cwd=ROOT)
        if result:raise SystemExit(result)
