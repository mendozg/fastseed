#!/usr/bin/env python3
"""Independent byte fixtures using Python struct/hashlib, never the C++ codec."""
from pathlib import Path
import argparse
import hashlib
import struct

def frame(command, payload):
    return bytes.fromhex('a2b2e2f2') + command.encode().ljust(12, b'\0') + struct.pack('<I', len(payload)) + hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4] + payload

ipv4 = bytes.fromhex('00000000000000000000ffffcb00710a')
ipv6 = bytes.fromhex('20010db8000000000000000000000001')
recv = struct.pack('<Q', 1) + ipv4 + struct.pack('>H', 8226)
sender = struct.pack('<Q', 0) + ipv6 + struct.pack('>H', 8226)
version = struct.pack('<iQq', 70016, 1, 1700000000) + recv + sender + struct.pack('<Q', 0x1122334455667788) + b'\x09/fixture/' + struct.pack('<iB', 42, 0)
payloads = {
    'verack': b'', 'sendaddrv2': b'', 'wtxidrelay': b'', 'getaddr': b'',
    'ping': struct.pack('<Q', 0x1122334455667788),
    'pong': struct.pack('<Q', 0x1122334455667788),
    'version': version,
    'addr': b'\x01' + struct.pack('<I', 1700000000) + recv,
    'addrv2': b'\x02' + struct.pack('<I', 1700000000) + b'\x01\x01\x04' + bytes.fromhex('cb00710a') + struct.pack('>H', 8226)
        + struct.pack('<I', 1700000001) + b'\x00\x02\x10' + ipv6 + struct.pack('>H', 8226),
}
text = '// Generated independently by generate_vectors.py; see ALPHA2-PROTOCOL.md.\n#pragma once\n#include <string_view>\nnamespace vectors {\n'
for name, payload in payloads.items():
    text += f'inline constexpr std::string_view {name}_payload = "{payload.hex()}";\n'
    text += f'inline constexpr std::string_view {name}_wire = "{frame(name, payload).hex()}";\n'
text += '}\n'
parser = argparse.ArgumentParser();parser.add_argument('--check', action='store_true');args=parser.parse_args()
path = Path(__file__).with_name('known_vectors.hpp')
if args.check:
    if path.read_text() != text: raise SystemExit('Known-byte vector mismatch')
else: path.write_text(text, encoding='utf-8', newline='\n')
print('PASS: nine independent protocol fixtures')
