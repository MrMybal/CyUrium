# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Normalize localized /showIncludes so Ninja records C/C++ header dependencies."""
import re,subprocess,sys
result=subprocess.run(sys.argv[1:],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
try:text=result.stdout.decode('utf-8')
except UnicodeDecodeError:text=result.stdout.decode('mbcs',errors='replace')
text=re.sub(r'^Remarque[\s\u00a0]*: inclusion du fichier[\s\u00a0]*:', 'Note: including file:', text, flags=re.MULTILINE)
sys.stdout.buffer.write(text.encode('utf-8'))
raise SystemExit(result.returncode)
