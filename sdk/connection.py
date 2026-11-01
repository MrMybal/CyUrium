# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Local account protected discovery. Never shared with pages or command lines."""
import ctypes
import json
import os
from pathlib import Path


def crypt(data, decrypt=False):
    if os.name != 'nt':
        return data  # POSIX mode 0700 directory / 0600 file, pending platform validation.
    from ctypes import wintypes
    class Blob(ctypes.Structure):
        _fields_=[('size',wintypes.DWORD),('data',ctypes.POINTER(ctypes.c_ubyte))]
    raw=ctypes.create_string_buffer(data)
    source=Blob(len(data),ctypes.cast(raw,ctypes.POINTER(ctypes.c_ubyte)))
    result=Blob()
    api=ctypes.WinDLL('crypt32',use_last_error=True)
    func=api.CryptUnprotectData if decrypt else api.CryptProtectData
    if not func(ctypes.byref(source),None,None,None,None,1,ctypes.byref(result)):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return ctypes.string_at(result.data,result.size)
    finally:
        ctypes.memset(result.data,0,result.size)
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.LocalFree.argtypes=[ctypes.c_void_p]
        kernel.LocalFree(result.data)


def save(path, value):
    path=Path(path)
    path.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
    tmp=path.with_suffix('.tmp')
    # Created under a private per-runtime directory. On Windows DPAPI additionally
    # restricts the discovery secret to the current OS account.
    with os.fdopen(os.open(tmp,os.O_CREAT|os.O_TRUNC|os.O_WRONLY,0o600),'wb') as stream:
        stream.write(crypt(json.dumps(value).encode('utf-8')))
    os.replace(tmp,path)


def load(path):
    value=json.loads(crypt(Path(path).read_bytes(),decrypt=True))
    from urllib.parse import urlsplit
    uri=urlsplit(value['url'])
    if uri.scheme!='http' or uri.hostname!='127.0.0.1' or uri.path!='/v1' or uri.username or uri.password or uri.query or uri.fragment:
        raise ValueError('Invalid local SDK endpoint')
    return value
