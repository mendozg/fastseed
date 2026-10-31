#!/usr/bin/env python3
"""Reject inconsistent release labels before building or packaging."""
from pathlib import Path
import re

source = Path(__file__).resolve().parents[1]
version = (source / 'release/VERSION').read_text().strip()
if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', version):
    raise SystemExit('Invalid release version')
base = version.split('-', 1)[0]
identity = f'BSTY-FastSeed {version} (Development)'
checks = {
    'CMakeLists.txt': f'project(BSTYFastSeed VERSION {base} LANGUAGES CXX)',
    'include/fastseed/core/version.hpp': f'version = "{version}";',
    'scripts/release-gate.sh': identity,
}
for path, expected in checks.items():
    if expected not in (source / path).read_text():
        raise SystemExit(f'Version mismatch: {path}')
header = (source / 'include/fastseed/core/version.hpp').read_text()
if f'version_identity = "{identity}";' not in header:
    raise SystemExit('Version identity mismatch')
distribution_version = source.parent / 'release/VERSION'
if distribution_version.exists() and distribution_version.read_text().strip() != version:
    raise SystemExit('Root and source release versions differ')
print(f'Version consistency PASS: {version}')
