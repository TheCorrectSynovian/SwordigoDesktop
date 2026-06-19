#!/bin/bash
# generate_manifest.sh — RPM/DEB post-install scriptlet
# Scans the engine/ directory and writes manifest.json
#
# Usage (from project root or package install):
#   ./builder/generate_manifest.sh [engine_dir] [output_path]
#
# Default:
#   engine_dir  = /usr/share/swordigo/engine
#   output_path = /usr/share/swordigo/engine/manifest.json
#
# Can also be run during development:
#   ./builder/generate_manifest.sh ./engine ./engine/manifest.json

set -euo pipefail

ENGINE_DIR="${1:-/usr/share/swordigo/engine}"
OUTPUT="${2:-${ENGINE_DIR}/manifest.json}"

# If the swordigo-desktop binary is available, use it to generate
# (it has the full SHA256 + KNOWN_HASHES logic built in)
SWORDIGO_BIN="${3:-/usr/bin/swordigo-desktop}"
if [ -x "$SWORDIGO_BIN" ]; then
    echo "[manifest] Using $SWORDIGO_BIN --generate-manifest"
    "$SWORDIGO_BIN" --generate-manifest "$ENGINE_DIR" "$OUTPUT"
    exit 0
fi

# Fallback: pure shell scriptlet (no SHA hashing, just lists files)
echo "[manifest] Generating manifest from $ENGINE_DIR → $OUTPUT"

# Known hashes for status lookup (same as KNOWN_HASHES in binary_selector.cpp)
declare -A KNOWN_VERSIONS=(
    ["cee15dd2730746269ce5db97d150371ebbad1f41371c6a728f1bb7d045632138"]="1.4.6:tested"
    ["08d49dd6f7f8639a4c59f290ff2bb79254accf710f530bf53c2fce1659191c9e"]="1.4.12:tested"
    ["5ae524abc08d4a1c8304d5faa8d55340bea1b191c5dcb0e68b35faeeae011368"]="1.4.6-patched:testing"
    ["a7c00ff6f3ed0d5b3221158d6e214bba03288c1e6782be3dc2c736ae80eb19df"]="6.6-rl:tested"
)

DEFAULT_BINARY=""
FIRST=true
INSTANCE_COUNT=0

# Start JSON
echo '{' > "$OUTPUT"

# Temp file for instances array
INSTANCES_TMP=$(mktemp)
echo '  "instances": [' > "$INSTANCES_TMP"

# Scan engine/VERSION_DIR/ARCH_DIR/libswordigo.so
for version_dir in "$ENGINE_DIR"/*/; do
    [ -d "$version_dir" ] || continue
    vdir=$(basename "$version_dir")
    
    # Skip trash/
    [ "$vdir" = "trash" ] && continue
    
    for arch_dir in "$version_dir"*/; do
        [ -d "$arch_dir" ] || continue
        adir=$(basename "$arch_dir")
        
        # Determine arch
        case "$adir" in
            armeabi-v7a) ARCH="ARM32" ;;
            arm64-v8a)   ARCH="ARM64" ;;
            *)           continue ;;
        esac
        
        # Find the .so file
        SO_FILE=""
        for f in "$arch_dir"libswordigo*.so; do
            [ -f "$f" ] && SO_FILE="$f" && break
        done
        [ -z "$SO_FILE" ] && continue
        
        FILENAME=$(basename "$SO_FILE")
        FILEPATH="$SO_FILE"
        FILESIZE=$(stat -c%s "$SO_FILE" 2>/dev/null || echo 0)
        
        # Compute SHA256
        SHA=$(sha256sum "$SO_FILE" 2>/dev/null | cut -d' ' -f1 || echo "")
        
        # Determine version and status from SHA
        VERSION="$vdir"
        STATUS="unknown"
        if [ -n "$SHA" ] && [ -n "${KNOWN_VERSIONS[$SHA]+x}" ]; then
            IFS=':' read -r VERSION STATUS <<< "${KNOWN_VERSIONS[$SHA]}"
        else
            # Extract version from directory name
            case "$vdir" in
                v*)    VERSION="${vdir#v}" ;;
                rl-v*) VERSION="${vdir#rl-v}-rl" ;;
                sw3d)  VERSION="sw3d" ;;
                *)     VERSION="$vdir" ;;
            esac
        fi
        
        # Determine game type and assets
        GAME_TYPE="Swordigo"
        ASSETS_DIR="assets"
        IS_RL=false
        case "$vdir" in
            rl-*)
                GAME_TYPE="RLSwordigo"
                ASSETS_DIR="rl_assets"
                IS_RL=true
                ;;
        esac
        
        # Build label
        STATUS_LABEL=$(echo "$STATUS" | sed 's/tested/Stable/;s/testing/Testing/;s/unknown/Unknown/')
        if $IS_RL; then
            LABEL="[RL] v${VERSION} [${ARCH}] (${STATUS_LABEL})"
        elif [ "$vdir" = "sw3d" ]; then
            LABEL="[3D] [${ARCH}] (${STATUS_LABEL})"
        else
            LABEL="v${VERSION} [${ARCH}] (${STATUS_LABEL})"
        fi
        
        # Dependencies (check for libmini.so and libGlossHook.so in arch dir)
        DEPS=""
        DEP_PATHS=""
        for dep in libmini.so libGlossHook.so; do
            if [ -f "${arch_dir}${dep}" ]; then
                [ -n "$DEPS" ] && DEPS="${DEPS}, "
                DEPS="${DEPS}\"${dep}\""
                [ -n "$DEP_PATHS" ] && DEP_PATHS="${DEP_PATHS}, "
                DEP_PATHS="${DEP_PATHS}\"${arch_dir}${dep}\""
            fi
        done
        
        # Set default (prefer v1.4.12 ARM32)
        IS_DEFAULT="false"
        if [ "$vdir" = "v1.4.12" ] && [ "$ARCH" = "ARM32" ]; then
            DEFAULT_BINARY="$FILEPATH"
            IS_DEFAULT="true"
        fi
        # Fallback: first binary found
        if [ -z "$DEFAULT_BINARY" ]; then
            DEFAULT_BINARY="$FILEPATH"
            IS_DEFAULT="true"
        fi
        
        # Write instance JSON
        if [ $INSTANCE_COUNT -gt 0 ]; then
            echo ',' >> "$INSTANCES_TMP"
        fi
        
        cat >> "$INSTANCES_TMP" << INSTANCE_EOF
    {
      "name": "${LABEL}",
      "filename": "${FILENAME}",
      "filepath": "${FILEPATH}",
      "sha256": "${SHA}",
      "version": "${VERSION}",
      "version_dir": "${vdir}",
      "arch": "${ARCH}",
      "file_size": ${FILESIZE},
      "is_default": ${IS_DEFAULT},
      "status": "${STATUS}",
      "game_type": "${GAME_TYPE}",
      "assets_dir": "${ASSETS_DIR}",
      "dependencies": [${DEPS}],
      "dep_paths": [${DEP_PATHS}]
    }
INSTANCE_EOF
        
        INSTANCE_COUNT=$((INSTANCE_COUNT + 1))
        echo "[manifest] Found: $LABEL → $FILEPATH ($FILESIZE bytes)"
    done
done

# Complete JSON
echo "  \"default\": \"${DEFAULT_BINARY}\"," >> "$OUTPUT"
cat "$INSTANCES_TMP" >> "$OUTPUT"
echo "" >> "$OUTPUT"
echo '  ]' >> "$OUTPUT"
echo '}' >> "$OUTPUT"

rm -f "$INSTANCES_TMP"

echo "[manifest] Generated manifest with $INSTANCE_COUNT instances → $OUTPUT"
