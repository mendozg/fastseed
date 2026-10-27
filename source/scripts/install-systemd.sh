#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "For existing installations use docs/DEPLOYMENT.md to preserve config and data."
exec bash "$ROOT/deploy-fastseed.sh" "$@"
