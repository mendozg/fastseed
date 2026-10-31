#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_TMP="$(mktemp -d "${TMPDIR:-/tmp}/fastseed-gate.XXXXXX")"
export TMPDIR="$TEST_TMP"
trap 'rm -rf -- "$TEST_TMP"' EXIT
BUILD="$ROOT/build-release"
LOGS="$ROOT/verification-results"
mkdir -p "$LOGS"
exec > >(tee "$LOGS/release.log") 2>&1
trap 'echo "Release verification FAILED at line $LINENO; no installation was performed." >&2' ERR
python3 "$ROOT/scripts/verify-version.py"
python3 "$ROOT/tests/protocol/generate_vectors.py" --check
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DFASTSEED_ENABLE_SANITIZERS=OFF -DFASTSEED_BUILD_FUZZ=OFF
cmake --build "$BUILD" -j"${JOBS:-2}"
ctest --test-dir "$BUILD" --output-on-failure --timeout 60
ctest --test-dir "$BUILD" -R '^(phase8_tests|phase101_probe_tests)$' --repeat until-fail:20 --output-on-failure --timeout 60
"$BUILD/bsty-fastseed" --version | grep -Fx 'BSTY-FastSeed 2.0.0-alpha2 (Development)'
"$BUILD/bsty-fastseed" --check-config "$ROOT/config/fastseed.example.toml"
"$BUILD/bsty-fastseed" --check-config "$ROOT/config/fastseed.production.toml"
python3 "$ROOT/tests/dns/test_dns_smoke.py" "$BUILD/bsty-fastseed"
printf '%s\n' 'BSTY-FastSeed v2.0.0-alpha2 full release gate: PASS'
