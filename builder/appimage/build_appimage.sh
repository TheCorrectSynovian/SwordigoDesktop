#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop — AppImage Builder
# Bundles: binary, assets, music, libswordigo.so, all shared
# library dependencies (libunicorn, SDL2, OpenAL, GL, etc.)
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
APPDIR="$SCRIPT_DIR/Swordigo.AppDir"
OUTPUT="$SCRIPT_DIR/Swordigo-x86_64.AppImage"

echo "============================================"
echo " Swordigo Desktop — AppImage Builder"
echo "============================================"
echo "[1/7] ROOT_DIR = $ROOT_DIR"

# ---------- Clean & prepare ----------
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/swordigo/assets/resources"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/licenses"

# ---------- Build ----------
echo "[2/7] Building swordigo_boot..."
cd "$ROOT_DIR"
make clean
make swordigo_boot
echo "      Binary size: $(du -h swordigo_boot | cut -f1)"

# ---------- Copy binary + game lib ----------
echo "[3/7] Copying binary and game library..."
cp swordigo_boot "$APPDIR/usr/bin/"
cp libswordigo.so "$APPDIR/usr/share/swordigo/"
cp LICENSE "$APPDIR/usr/share/licenses/LICENSE"

# ---------- Copy ALL assets (textures, models, sounds, music) ----------
echo "[4/7] Copying game assets (this may take a moment)..."
cp -r assets/resources/* "$APPDIR/usr/share/swordigo/assets/resources/"
echo "      Assets: $(find "$APPDIR/usr/share/swordigo/assets" -type f | wc -l) files"
echo "      Assets size: $(du -sh "$APPDIR/usr/share/swordigo/assets" | cut -f1)"

# ---------- Bundle ALL shared library dependencies ----------
echo "[5/7] Bundling shared library dependencies..."

# Get all direct dependencies
DEPS=$(ldd swordigo_boot | grep "=> /" | awk '{print $3}' | sort -u)

# Libraries to SKIP (system-level that AppImage should NOT bundle)
# These are provided by the host's graphics driver / glibc
SKIP_PATTERN="linux-vdso|ld-linux|libc.so|libm.so|libdl.so|libpthread|librt.so|libgcc_s|libstdc\+\+|libnsl|libresolv|libBrokenLocale|libSegFault|libthread_db|libutil"
# Also skip driver-specific GL libraries (mesa/nvidia/amd provide these)
SKIP_PATTERN="$SKIP_PATTERN|libGL.so|libGLX|libGLdispatch|libEGL|libdrm|libX11|libxcb|libXext|libXau|libXdmcp|libXi|libXfixes|libXrandr|libXrender|libXcursor|libXss|libXinerama|libwayland|libdbus|libsystemd|libcap|libgcrypt|libgpg|liblzma|libzstd|liblz4|libelf|libdw|libunwind|libbsd"

BUNDLED=0
for lib in $DEPS; do
    libname=$(basename "$lib")
    if echo "$libname" | grep -qE "$SKIP_PATTERN"; then
        continue
    fi
    cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null && BUNDLED=$((BUNDLED+1)) || true
done

# Also explicitly bundle these critical libraries if not caught above
for critical_lib in libunicorn libopenal libSDL2 libsndio libpulse libsamplerate libasound libvorbis libogg libFLAC libopus libsndfile libmpg123 libzlib libz.so libpulsecommon libpulse-simple libasyncns; do
    FOUND=$(ldconfig -p 2>/dev/null | grep "$critical_lib" | head -n1 | awk '{print $NF}')
    if [ -n "$FOUND" ] && [ -f "$FOUND" ] && [ ! -f "$APPDIR/usr/lib/$(basename $FOUND)" ]; then
        cp -L "$FOUND" "$APPDIR/usr/lib/" 2>/dev/null && BUNDLED=$((BUNDLED+1)) || true
    fi
done

echo "      Bundled $BUNDLED libraries"
echo "      Libs size: $(du -sh "$APPDIR/usr/lib" | cut -f1)"
ls -la "$APPDIR/usr/lib/" | head -20

# ---------- Desktop integration ----------
echo "[6/7] Setting up desktop integration..."

# Desktop entry for AppDir root
cat > "$APPDIR/Swordigo.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Swordigo Desktop
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop Port — ARM Emulation Engine
Keywords=swordigo;game;rpg;adventure;sword;
DESKTOP
cp "$APPDIR/Swordigo.desktop" "$APPDIR/usr/share/applications/"

# Copy the real game icon
cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/swordigo-desktop.png"
cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png"
cp "$ROOT_DIR/src/assets/icon_gnome.png" "$APPDIR/.DirIcon"

# AppRun script — the entry point
cat > "$APPDIR/AppRun" << 'APPRUN'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=$(dirname "$SELF")

# Point to bundled assets and game library
export SWORDIGO_DATA_DIR="$HERE/usr/share/swordigo/"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"

# Use bundled OpenAL if available
if [ -f "$HERE/usr/lib/libopenal.so.1" ]; then
    export OPENAL_DRIVERS=pulse,alsa,oss,null
fi

# Launch
exec "$HERE/usr/bin/swordigo_boot" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# ---------- Package with appimagetool ----------
echo "[7/7] Packaging AppImage..."
cd "$SCRIPT_DIR"

APPIMAGETOOL="$SCRIPT_DIR/appimagetool"
if [ ! -f "$APPIMAGETOOL" ]; then
    echo "      Downloading appimagetool..."
    wget -q "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" -O "$APPIMAGETOOL"
    chmod +x "$APPIMAGETOOL"
fi

echo "      AppDir size: $(du -sh "$APPDIR" | cut -f1)"
echo "      Packaging..."
ARCH=x86_64 "$APPIMAGETOOL" --no-appstream "$APPDIR" "$OUTPUT" 2>&1 | tail -5

echo ""
echo "============================================"
echo " ✅ AppImage built successfully!"
echo " 📦 Output: $OUTPUT"
echo " 📏 Size:   $(du -h "$OUTPUT" | cut -f1)"
echo "============================================"
echo ""
echo "Run with:  chmod +x $(basename $OUTPUT) && ./$(basename $OUTPUT)"
