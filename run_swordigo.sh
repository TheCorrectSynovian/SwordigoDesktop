#!/bin/bash
# run_swordigo.sh — One-shot build, install, and run
# Usage: ./run_swordigo.sh [--clean] [--sre-only] [--no-build]

set -e
cd ~/SwordigoDesktop

CLEAN=0
SRE_ONLY=0
NO_BUILD=0

for arg in "$@"; do
    case $arg in
        --clean)    CLEAN=1 ;;
        --sre-only) SRE_ONLY=1 ;;
        --no-build) NO_BUILD=1 ;;
        --help)
            echo "Usage: ./run_swordigo.sh [--clean] [--sre-only] [--no-build]"
            echo "  --clean     Full clean rebuild"
            echo "  --sre-only  Only rebuild libsre.so"
            echo "  --no-build  Skip build, just install and run"
            exit 0 ;;
    esac
done

ENGINE_DIR="$HOME/.local/share/swordigo-desktop/engine/v1.4.12/arm64-v8a"

# ---- Build ----
if [ $NO_BUILD -eq 0 ]; then
    if [ $CLEAN -eq 1 ]; then
        echo "=== CLEAN BUILD ==="
        make clean
    fi

    if [ $SRE_ONLY -eq 1 ]; then
        echo "=== Building libsre.so only ==="
        make libsre.so
    else
        echo "=== Building everything ==="
        make -j$(nproc)
        echo "=== Building asset_viewer ==="
        make asset_viewer
    fi
fi

# ---- Install libsre.so to ALL ARM64 engine directories ----
if [ -f libsre.so ]; then
    SRE_INSTALLED=0
    for dir in "$HOME/.local/share/swordigo-desktop/engine"/*/arm64-v8a; do
        if [ -d "$dir" ]; then
            cp libsre.so "$dir/libsre.so"
            echo "[OK] libsre.so -> $dir/"
            SRE_INSTALLED=$((SRE_INSTALLED + 1))
        fi
    done
    if [ $SRE_INSTALLED -eq 0 ]; then
        # Fallback: ensure at least the primary engine dir gets it
        mkdir -p "$ENGINE_DIR"
        cp libsre.so "$ENGINE_DIR/libsre.so"
        echo "[OK] libsre.so -> $ENGINE_DIR/ (fallback)"
    else
        echo "[OK] libsre.so installed to $SRE_INSTALLED ARM64 engine dir(s)"
    fi
else
    echo "[WARN] libsre.so not found — skipping install"
fi

# ---- Install updated manifest.json ----
if [ -f engine/manifest.json ]; then
    MANIFEST_DIR="$HOME/.local/share/swordigo-desktop/engine"
    mkdir -p "$MANIFEST_DIR"
    cp engine/manifest.json "$MANIFEST_DIR/manifest.json"
    echo "[OK] manifest.json -> $MANIFEST_DIR/"
fi

# ---- Run ----
echo ""
echo "=========================================="
echo "  Launching Swordigo Desktop (ARM64)"
echo "=========================================="
echo ""
exec ./swordigo_boot "$@"
