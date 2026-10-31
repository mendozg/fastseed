#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_TMP="$(mktemp -d "${TMPDIR:-/tmp}/fastseed-gate.XXXXXX")"
export TMPDIR="$TEST_TMP"
trap 'rm -rf -- "$TEST_TMP"' EXIT
BUILD="$ROOT/build-sanitizers"
LOGS="$ROOT/verification-results"
mkdir -p "$LOGS"
exec > >(tee "$LOGS/sanitizers.log") 2>&1
trap 'echo "Sanitizer verification FAILED at line $LINENO." >&2' ERR
python3 "$ROOT/scripts/verify-version.py"
python3 "$ROOT/tests/protocol/generate_vectors.py" --check
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DFASTSEED_ENABLE_SANITIZERS=ON -DFASTSEED_BUILD_FUZZ=OFF
cmake --build "$BUILD" -j"${JOBS:-2}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
ctest --test-dir "$BUILD" --output-on-failure --timeout 60
printf '%s\n' 'BSTY-FastSeed v2.0.0-alpha2 full sanitizer gate: PASS'
