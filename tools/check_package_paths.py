# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Reject local build/profile paths in distributable files without logging them."""
from pathlib import Path
import os
import sys

ROOT = Path(__file__).resolve().parents[1]


def check_package_paths(package):
    roots = {str(ROOT.resolve())}
    for name in ('USERPROFILE', 'HOME'):
        value = os.environ.get(name)
        if value and len(value) > 3:
            roots.add(value)
    needles = set()
    for root in roots:
        for value in (root, root.replace('\\', '/'), root.replace('/', '\\')):
            for encoding in ('utf-8', 'utf-16le'):
                needles.add(value.encode(encoding).lower())
    overlap_size = max(map(len, needles))
    findings = []
    for path in Path(package).rglob('*'):
        if not path.is_file():
            continue
        tail = b''
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b''):
                data = (tail + chunk).lower()
                if any(needle in data for needle in needles):
                    findings.append(path.relative_to(package).as_posix())
                    break
                tail = data[-overlap_size:]
    if findings:
        raise RuntimeError('Local build/profile path in package files: '+', '.join(findings[:10]))


if __name__ == '__main__':
    check_package_paths(Path(sys.argv[1]))
