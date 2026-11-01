# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Regenerate desktop GPU/icon exports from the original CyVR Urium PNG."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]/'assets/branding'
with Image.open(root/'urium-logo.png') as source:
    image=source.convert('RGBA').resize((256,256),Image.Resampling.LANCZOS)
    (root/'urium-logo.rgba').write_bytes(image.tobytes())
    source.save(root/'urium.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
print('Original Urium logo exported for desktop GPU rendering and Windows icons.')
