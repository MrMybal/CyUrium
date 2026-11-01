# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Windows session codec for isolated QA fixtures; uses the current user's DPAPI."""
import ctypes
from ctypes import wintypes
import json
PREFIX = b'CYURIUM-DPAPI-1\n'

class Blob(ctypes.Structure):
    _fields_ = [('size', wintypes.DWORD), ('data', ctypes.POINTER(ctypes.c_ubyte))]

def transform(data, protect):
    crypto = ctypes.WinDLL('crypt32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    source = ctypes.create_string_buffer(data)
    input_blob = Blob(len(data), ctypes.cast(source, ctypes.POINTER(ctypes.c_ubyte)))
    output = Blob()
    function = crypto.CryptProtectData if protect else crypto.CryptUnprotectData
    function.argtypes = [ctypes.POINTER(Blob), ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(Blob)]
    function.restype = wintypes.BOOL
    if not function(ctypes.byref(input_blob), None, None, None, None, 1, ctypes.byref(output)):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return ctypes.string_at(output.data, output.size)
    finally:
        ctypes.memset(output.data, 0, output.size)
        kernel.LocalFree(output.data)

def read_session(path):
    data = path.read_bytes()
    assert data.startswith(PREFIX), 'Expected a DPAPI-protected session'
    return json.loads(transform(data[len(PREFIX):], False))

def write_session(path, data):
    path.write_bytes(PREFIX + transform(json.dumps(data).encode(), True))
