# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Run SDK integration tests with the exact private Python shipped in the app."""
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    import sys
    raise SystemExit(subprocess.call([str(ROOT/'dist/windows/CyUrium/sdk/python/python.exe'),'-I','-B',
        str(ROOT/'tests/sdk_integration.py'),str(ROOT),*sys.argv[1:]],cwd=ROOT))
