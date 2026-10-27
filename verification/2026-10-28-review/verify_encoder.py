"""Compile the current C encoder and SDK Mbed TLS body; check boundary guards."""
import ast
import base64
import ctypes as C
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / 'build'
BUILD.mkdir(exist_ok=True)
legacy = ast.parse((ROOT / 'verification/2026-10-26/run_checks.py').read_text())
prelude = next(ast.literal_eval(n.value) for n in legacy.body if isinstance(n, ast.Assign)
               and any(isinstance(t, ast.Name) and t.id == 'prelude' for t in n.targets))
prelude += '\n#define ESP_FAIL -1\n'

def strip(text):
    return re.sub(r'^\s*#include[^\n]*', '', text, flags=re.M)

def function(text, name):
    match = re.search(r'(?m)^int\s+' + name + r'\s*\(', text)
    start = text.index('{', match.end())
    depth, end = 1, start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]

sdk = Path(r'D:/esp/v6.0.1/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/drivers/builtin/src/base64.c')
encoder = ROOT / 'Components/Svc/Svc_Encode_Base64'
source = prelude + strip((encoder / 'Svc_Encode_Base64.h').read_text())
source += function(sdk.read_text(), 'mbedtls_base64_encode')
source += strip((encoder / 'Svc_Encode_Base64.c').read_text())
source += '''
EXPORT size_t capacity(void){return MAX_ENCODER_LEN;}
EXPORT int encode(unsigned char* input,size_t size,unsigned char* output){
    svc_frame_packet_t p={input,size};return svc_encode_base64_with_jsonfilled(&p,output);
}
EXPORT int null_packet(unsigned char* output){return svc_encode_base64_with_jsonfilled(NULL,output);}
'''
(BUILD / 'encoder.c').write_text(source)
cl = Path(r'D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe')
for command in ([str(cl), '/nologo', '/utf-8', '/c', '/Od', '/Oi-', '/GS-', '/Zl',
                 '/Fo' + str(BUILD / 'encoder.obj'), str(BUILD / 'encoder.c')],
                [str(cl.with_name('link.exe')), '/nologo', '/dll', '/noentry', '/nodefaultlib',
                 '/out:' + str(BUILD / 'encoder.dll'), str(BUILD / 'encoder.obj')]):
    proc = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if proc.returncode:
        raise RuntimeError(proc.stdout + proc.stderr)
dll = C.CDLL(str(BUILD / 'encoder.dll'))
dll.capacity.restype = C.c_size_t
dll.encode.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]
dll.null_packet.argtypes = [C.c_void_p]
capacity = dll.capacity()
results = []
for size in (1, 2, 3, 30000, 30717, 30718, 30719, 30720, 30721, 1000000, 0):
    valid = 0 < size <= 30720
    data = bytes(i % 251 for i in range(min(size, 30721)))
    inp = C.create_string_buffer(data)
    out = (C.c_ubyte * (capacity + 32))(*([0xA5] * (capacity + 32)))
    ret = dll.encode(inp, size, out)
    intact = bytes(out)[capacity:] == b'\xA5' * 32
    if valid:
        parsed = json.loads(bytes(out).split(b'\0', 1)[0])
        ok = ret == 0 and base64.b64decode(parsed['params']['image'], validate=True) == data
    else:
        ok = ret != 0 and out[0] == 0
    results.append({'frame_bytes': size, 'passed': ok and intact, 'guard_intact': intact})
out = C.create_string_buffer(capacity)
results.append({'case': 'null packet', 'passed': dll.null_packet(out) != 0 and out.value == b''})
results.append({'case': 'null payload', 'passed': dll.encode(None, 1, out) != 0 and out.value == b''})
results.append({'case': 'null output', 'passed': dll.encode(None, 1, None) != 0})
summary = {'scope': 'Current C encoder and SDK Mbed TLS encoder, host execution with guard bytes; no hardware.',
           'allocation': capacity, 'passed': sum(r['passed'] for r in results),
           'failed': sum(not r['passed'] for r in results), 'checks': results}
(HERE / 'encoder-tests.json').write_text(json.dumps(summary, indent=2))
print(json.dumps(summary, indent=2))
raise SystemExit(bool(summary['failed']))
