#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DFASTSEED_BUILD_FUZZ=OFF
cmake --build "$BUILD" --target phase12_intelligence_tests phase13_tests phase13_interop_tests phase13_release_tests bsty-fastseed -j"${JOBS:-4}"
"$BUILD/phase12_intelligence_tests"
"$BUILD/phase13_tests"
"$BUILD/phase13_interop_tests"
"$BUILD/phase13_release_tests"
test "$("$BUILD/bsty-fastseed" --version)" = "BSTY-FastSeed 1.0.0 (Production)"
"$BUILD/bsty-fastseed" --self-test
"$BUILD/bsty-fastseed" --check-config "$ROOT/config/fastseed.production.toml"
printf '%s\n' 'BSTY-FastSeed v1.0.0 production release gate: PASS'
