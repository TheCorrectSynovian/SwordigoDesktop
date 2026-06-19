#!/bin/bash
# Swordigo Desktop — Build & Run
# Usage:
#   ./run_swordigo.sh          # Build + Run
#   ./run_swordigo.sh --run    # Run only (skip build)
#   ./run_swordigo.sh --build  # Build only (skip run)

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

BUILD=true
RUN=true

case "${1:-}" in
    --run)   BUILD=false ;;
    --build) RUN=false ;;
esac

if $BUILD; then
    echo "=== Building Swordigo Desktop ==="
    make -j$(nproc)
    echo "=== Build OK ==="
fi

if $RUN; then
    echo "=== Launching ==="
    ./swordigo_boot "$@"
fi
