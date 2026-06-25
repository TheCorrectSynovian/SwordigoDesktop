#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop v7.0 — AppImage Builder
# ============================================================
# Bundles EVERYTHING into a single portable AppImage:
#   - swordigo_boot + asset_viewer + libsre.so
#   - Engine binaries (v1.4.6 + v1.4.12, ARM32 + ARM64)
#   - Game assets (942 files), music (9 files)
#   - Launcher textures, fonts, icons
#   - All shared library dependencies
#
# On first run: copies engine/, assets/, res/, src/assets/
# to ~/.local/share/swordigo-desktop/ for user-writable access.
# Subsequent runs detect existing data and skip the copy.
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
APPDIR="$SCRIPT_DIR/Swordigo.AppDir"
OUTPUT="$SCRIPT_DIR/Swordigo-v7.0-x86_64.AppImage"

# Where built binaries live (ext4)
EXT4_DIR="$HOME/SwordigoDesktop"
# Where game data lives
USER_DATA="$HOME/.local/share/swordigo-desktop"

# Shipped engine versions
SHIP_VERSIONS=("v1.4.6" "v1.4.12")

echo "============================================"
echo " Swordigo Desktop v7.0 — AppImage Builder"
echo "============================================"
echo "  Root:     $ROOT_DIR"
echo "  Ext4:     $EXT4_DIR"
echo "  Data:     $USER_DATA"
echo "  Output:   $OUTPUT"
echo ""

# ============================================================
# Pre-flight checks
# ============================================================
echo "[0/7] Pre-flight checks..."

for bin in swordigo_boot libsre.so; do
    if [ ! -f "$EXT4_DIR/$bin" ]; then
        echo "  ❌ $EXT4_DIR/$bin not found — run 'make' on ext4 first!"
        exit 1
    fi
done
echo "      ✓ swordigo_boot ($(du -h "$EXT4_DIR/swordigo_boot" | cut -f1))"
echo "      ✓ libsre.so ($(du -h "$EXT4_DIR/libsre.so" | cut -f1))"

if [ -f "$EXT4_DIR/asset_viewer" ]; then
    echo "      ✓ asset_viewer ($(du -h "$EXT4_DIR/asset_viewer" | cut -f1))"
fi

if [ ! -d "$USER_DATA/assets/resources" ]; then
    echo "  ❌ $USER_DATA/assets/resources/ not found!"
    exit 1
fi
echo "      ✓ $(find "$USER_DATA/assets/resources" -type f | wc -l) game assets"

for ver in "${SHIP_VERSIONS[@]}"; do
    for arch in armeabi-v7a arm64-v8a; do
        so="$USER_DATA/engine/$ver/$arch/libswordigo.so"
        [ -f "$so" ] && echo "      ✓ engine/$ver/$arch/libswordigo.so" || echo "  ⚠ engine/$ver/$arch missing"
    done
done

MUSIC_COUNT=0
if [ -d "$USER_DATA/res/raw" ]; then
    MUSIC_COUNT=$(find "$USER_DATA/res/raw" -type f | wc -l)
    echo "      ✓ $MUSIC_COUNT music files"
fi
echo ""

# ============================================================
# Step 1: Clean & prepare AppDir
# ============================================================
echo "[1/7] Preparing AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/swordigo-desktop"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# ============================================================
# Step 2: Copy binaries
# ============================================================
echo "[2/7] Copying binaries..."
cp "$EXT4_DIR/swordigo_boot" "$APPDIR/usr/bin/"
chmod 755 "$APPDIR/usr/bin/swordigo_boot"

if [ -f "$EXT4_DIR/asset_viewer" ]; then
    cp "$EXT4_DIR/asset_viewer" "$APPDIR/usr/bin/"
    chmod 755 "$APPDIR/usr/bin/asset_viewer"
fi

cp "$EXT4_DIR/libsre.so" "$APPDIR/usr/share/swordigo-desktop/"
echo "      ✓ Binaries copied"

# ============================================================
# Step 3: Copy engine binaries + manifest
# ============================================================
echo "[3/7] Copying engine binaries..."

for ver in "${SHIP_VERSIONS[@]}"; do
    for arch_dir in "$USER_DATA/engine/$ver"/armeabi-v7a "$USER_DATA/engine/$ver"/arm64-v8a; do
        [ -d "$arch_dir" ] || continue
        arch_name=$(basename "$arch_dir")
        dest="$APPDIR/usr/share/swordigo-desktop/engine/$ver/$arch_name"
        mkdir -p "$dest"
        cp "$arch_dir"/*.so "$dest/" 2>/dev/null && \
            echo "      ✓ engine/$ver/$arch_name/ ($(ls "$arch_dir"/*.so | wc -l) .so files)"
    done
done

# Copy libsre.so into each ARM64 engine dir
for ver in "${SHIP_VERSIONS[@]}"; do
    arm64_dest="$APPDIR/usr/share/swordigo-desktop/engine/$ver/arm64-v8a"
    if [ -d "$arm64_dest" ]; then
        cp "$EXT4_DIR/libsre.so" "$arm64_dest/"
    fi
done
echo "      ✓ libsre.so → all ARM64 engine dirs"

# Generate manifest
cat > "$APPDIR/usr/share/swordigo-desktop/engine/manifest.json" << 'MANIFEST'
{
  "default": "engine/v1.4.12/arm64-v8a/libswordigo.so",
  "instances": [
    {
      "name": "v1.4.12 [ARM64] (Recommended)",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.12/arm64-v8a/libswordigo.so",
      "sha256": "f847814d1b6f81268567ed5ec2473fea4d4ee3b75d2c6fec7057227225e989f8",
      "version": "1.4.12",
      "version_dir": "v1.4.12",
      "arch": "ARM64",
      "file_size": 7230152,
      "is_default": true,
      "status": "tested",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "dependencies": [],
      "dep_paths": []
    },
    {
      "name": "v1.4.12 [ARM32] (Stable)",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.12/armeabi-v7a/libswordigo.so",
      "sha256": "08d49dd6f7f8639a4c59f290ff2bb79254accf710f530bf53c2fce1659191c9e",
      "version": "1.4.12",
      "version_dir": "v1.4.12",
      "arch": "ARM32",
      "file_size": 4607136,
      "is_default": false,
      "status": "tested",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "dependencies": [],
      "dep_paths": []
    },
    {
      "name": "v1.4.6 [ARM64] (Unknown)",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.6/arm64-v8a/libswordigo.so",
      "sha256": "a05a9a9814d5e29bb013fade46fa3011094261568674b7ccc4823fd8260ecb32",
      "version": "1.4.6",
      "version_dir": "v1.4.6",
      "arch": "ARM64",
      "file_size": 7221960,
      "is_default": false,
      "status": "unknown",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "dependencies": [],
      "dep_paths": []
    },
    {
      "name": "v1.4.6 [ARM32] (Stable)",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.6/armeabi-v7a/libswordigo.so",
      "sha256": "cee15dd2730746269ce5db97d150371ebbad1f41371c6a728f1bb7d045632138",
      "version": "1.4.6",
      "version_dir": "v1.4.6",
      "arch": "ARM32",
      "file_size": 4603040,
      "is_default": false,
      "status": "tested",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "dependencies": [],
      "dep_paths": []
    }
  ]
}
MANIFEST
echo "      ✓ manifest.json (4 instances)"

# ============================================================
# Step 4: Copy game assets + music + launcher textures
# ============================================================
echo "[4/7] Copying game assets (this may take a moment)..."

mkdir -p "$APPDIR/usr/share/swordigo-desktop/assets"
cp -r "$USER_DATA/assets/resources" "$APPDIR/usr/share/swordigo-desktop/assets/"
echo "      ✓ $(find "$APPDIR/usr/share/swordigo-desktop/assets" -type f | wc -l) game assets"

if [ -d "$USER_DATA/res/raw" ]; then
    mkdir -p "$APPDIR/usr/share/swordigo-desktop/res/raw"
    cp "$USER_DATA/res/raw/"* "$APPDIR/usr/share/swordigo-desktop/res/raw/" 2>/dev/null || true
    echo "      ✓ $(find "$APPDIR/usr/share/swordigo-desktop/res/raw" -type f | wc -l) music files"
fi

# Launcher textures & fonts (stored in launcher/ for clean user access)
mkdir -p "$APPDIR/usr/share/swordigo-desktop/launcher"
cp -r "$ROOT_DIR/src/assets/"* "$APPDIR/usr/share/swordigo-desktop/launcher/" 2>/dev/null || true
echo "      ✓ Launcher assets (icons, fonts, textures)"

# ============================================================
# Step 5: Bundle shared library dependencies
# ============================================================
echo "[5/7] Bundling shared library dependencies..."

DEPS=$(ldd "$EXT4_DIR/swordigo_boot" | grep "=> /" | awk '{print $3}' | sort -u)

# Skip system-level libs that the host provides
SKIP_PATTERN="linux-vdso|ld-linux|libc.so|libm.so|libdl.so|libpthread|librt.so|libgcc_s|libstdc\+\+|libnsl|libresolv"
SKIP_PATTERN="$SKIP_PATTERN|libGL.so|libGLX|libGLdispatch|libEGL|libdrm|libX11|libxcb|libXext|libXau|libXdmcp|libXi|libXfixes|libXrandr|libXrender|libXcursor|libXss|libXinerama|libwayland|libdbus|libsystemd|libcap|libgcrypt|libgpg|liblzma|libzstd|liblz4|libelf|libdw|libunwind|libbsd"

BUNDLED=0
for lib in $DEPS; do
    libname=$(basename "$lib")
    if echo "$libname" | grep -qE "$SKIP_PATTERN"; then
        continue
    fi
    cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null && BUNDLED=$((BUNDLED+1)) || true
done

# Explicitly bundle critical libs
for critical_lib in libunicorn libopenal libSDL3 libSDL3_image libsndio libpulse libsamplerate libasound libvorbis libvorbisfile libogg libFLAC libopus libsndfile libmpg123 libz.so libpulsecommon libpulse-simple libasyncns; do
    FOUND=$(ldconfig -p 2>/dev/null | grep "$critical_lib" | head -n1 | awk '{print $NF}')
    if [ -n "$FOUND" ] && [ -f "$FOUND" ] && [ ! -f "$APPDIR/usr/lib/$(basename $FOUND)" ]; then
        cp -L "$FOUND" "$APPDIR/usr/lib/" 2>/dev/null && BUNDLED=$((BUNDLED+1)) || true
    fi
done

echo "      ✓ Bundled $BUNDLED libraries ($(du -sh "$APPDIR/usr/lib" | cut -f1))"

# ============================================================
# Step 6: Desktop integration + AppRun with first-run setup
# ============================================================
echo "[6/7] Setting up desktop integration..."

# Desktop entry
cat > "$APPDIR/Swordigo.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Swordigo Desktop
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop v7.0 — Native Linux runtime with Dynarmic JIT
Keywords=swordigo;game;rpg;adventure;
StartupWMClass=Swordigo
DESKTOP
cp "$APPDIR/Swordigo.desktop" "$APPDIR/usr/share/applications/"

# Icons
if [ -f "$ROOT_DIR/src/assets/icon_gnome.png" ]; then
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/swordigo-desktop.png"
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png"
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/.DirIcon"
fi

# AppRun — entry point with first-run data setup
cat > "$APPDIR/AppRun" << 'APPRUN'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=$(dirname "$SELF")

# ---- Library path ----
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"

# ---- OpenAL config ----
if [ -f "$HERE/usr/lib/libopenal.so.1" ]; then
    export OPENAL_DRIVERS=pulse,alsa,oss,null
fi

# ---- First-run data setup ----
DEST="$HOME/.local/share/swordigo-desktop"
BUNDLED="$HERE/usr/share/swordigo-desktop"
MARKER="$DEST/.appimage-v7.0-installed"

if [ ! -f "$MARKER" ]; then
    echo "[Swordigo] First run detected — installing game data..."
    echo "           Source: $BUNDLED"
    echo "           Dest:   $DEST"
    mkdir -p "$DEST"

    # Copy engine (ARM binaries + libsre.so + manifest)
    if [ -d "$BUNDLED/engine" ]; then
        cp -rn "$BUNDLED/engine" "$DEST/" 2>/dev/null || cp -ru "$BUNDLED/engine" "$DEST/"
        echo "  ✓ Engine binaries"
    fi

    # Always update manifest (may have new entries)
    if [ -f "$BUNDLED/engine/manifest.json" ]; then
        cp "$BUNDLED/engine/manifest.json" "$DEST/engine/manifest.json"
        echo "  ✓ Manifest"
    fi

    # Copy assets
    if [ -d "$BUNDLED/assets" ]; then
        cp -rn "$BUNDLED/assets" "$DEST/" 2>/dev/null || cp -ru "$BUNDLED/assets" "$DEST/"
        echo "  ✓ Game assets"
    fi

    # Copy music
    if [ -d "$BUNDLED/res" ]; then
        cp -rn "$BUNDLED/res" "$DEST/" 2>/dev/null || cp -ru "$BUNDLED/res" "$DEST/"
        echo "  ✓ Music"
    fi

    # Copy launcher assets to dedicated launcher/ folder
    if [ -d "$BUNDLED/launcher" ]; then
        mkdir -p "$DEST/launcher"
        cp -rn "$BUNDLED/launcher/"* "$DEST/launcher/" 2>/dev/null || true
        echo "  ✓ Launcher assets"
    fi

    # Copy libsre.so to root (backup copy)
    if [ -f "$BUNDLED/libsre.so" ]; then
        cp "$BUNDLED/libsre.so" "$DEST/libsre.so"
    fi

    # Create user dirs
    mkdir -p "$DEST/save" "$DEST/cache" "$DEST/mods"

    # Mark as installed for this version
    echo "v7.0.0 installed $(date)" > "$MARKER"
    echo ""
    echo "[Swordigo] Setup complete! Launching game..."
    echo ""
else
    # Quick check: update manifest silently if bundled is newer
    if [ -f "$BUNDLED/engine/manifest.json" ]; then
        cp "$BUNDLED/engine/manifest.json" "$DEST/engine/manifest.json" 2>/dev/null || true
    fi
fi

# ---- Point data path to user dir ----
export SWORDIGO_DATA_DIR="$DEST/"

# ---- Launch ----
exec "$HERE/usr/bin/swordigo_boot" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

echo "      ✓ AppRun with first-run data setup"

# ============================================================
# Step 7: Package with appimagetool
# ============================================================
echo "[7/7] Packaging AppImage..."

TOTAL_FILES=$(find "$APPDIR" -type f | wc -l)
echo "      AppDir: $TOTAL_FILES files ($(du -sh "$APPDIR" | cut -f1))"

APPIMAGETOOL="$SCRIPT_DIR/appimagetool"
if [ ! -f "$APPIMAGETOOL" ]; then
    echo "  ❌ appimagetool not found at $APPIMAGETOOL"
    echo "     Download from: https://github.com/AppImage/appimagetool/releases"
    exit 1
fi

echo "      Packaging (this takes a while)..."
ARCH=x86_64 "$APPIMAGETOOL" --no-appstream "$APPDIR" "$OUTPUT" 2>&1 | tail -5

echo ""
echo "============================================"
echo " ✅ AppImage built successfully!"
echo " 📦 Output: $OUTPUT"
echo " 📏 Size:   $(du -h "$OUTPUT" | cut -f1)"
echo "============================================"
echo ""
echo "Run with:"
echo "  chmod +x $(basename $OUTPUT)"
echo "  ./$(basename $OUTPUT)"
echo ""
echo "First launch will install game data to:"
echo "  ~/.local/share/swordigo-desktop/"
echo ""
