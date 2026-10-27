#!/usr/bin/env python3
"""Verify packaged source bytes before editing configuration or building."""
from pathlib import Path
import hashlib
import sys
root=Path(__file__).resolve().parent.parent
manifest=root/'release/SOURCE-SHA256SUMS.txt'
failures=[]
seen=set()
for line in manifest.read_text(encoding='utf-8').splitlines():
    digest,relative=line.split('  ',1)
    path=(root/relative).resolve()
    if root not in path.parents:
        failures.append(relative+' (invalid path)');continue
    seen.add(relative)
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
        failures.append(relative)
if failures:
    print('Source integrity FAILED:\n'+'\n'.join(failures),file=sys.stderr)
    sys.exit(1)
print(f'Source integrity PASS: {len(seen)} files match packaged hashes.')
