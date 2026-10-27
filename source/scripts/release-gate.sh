#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
LOGS="$ROOT/verification-results"
mkdir -p "$LOGS"
exec > >(tee "$LOGS/release.log") 2>&1
trap 'echo "Release verification FAILED at line $LINENO; no installation was performed." >&2' ERR
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DFASTSEED_ENABLE_SANITIZERS=OFF -DFASTSEED_BUILD_FUZZ=OFF
cmake --build "$BUILD" -j"${JOBS:-2}"
ctest --test-dir "$BUILD" --output-on-failure --timeout 60
ctest --test-dir "$BUILD" -R '^(phase8_tests|phase101_probe_tests)$' --repeat until-fail:20 --output-on-failure --timeout 60
"$BUILD/bsty-fastseed" --version | grep -Fx 'BSTY-FastSeed 1.0.0 (Production)'
"$BUILD/bsty-fastseed" --check-config "$ROOT/config/fastseed.example.toml"
"$BUILD/bsty-fastseed" --check-config "$ROOT/config/fastseed.production.toml"
python3 "$ROOT/tests/dns/test_dns_smoke.py" "$BUILD/bsty-fastseed"
printf '%s\n' 'BSTY-FastSeed v1.0.0 full release gate: PASS'
