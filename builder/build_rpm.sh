#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop v3.0r — Standalone RPM Builder
# Packages: binary, ALL assets, music, game .so files,
# launcher textures, desktop entry, icon
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="3.0.0"
RELEASE="1"
PKG_NAME="swordigo-desktop"
ARCH="x86_64"

STAGING="/tmp/${PKG_NAME}-${VERSION}-staging"
RPM_OUT="/tmp/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"

echo "============================================"
echo " Swordigo Desktop v3.0r — RPM Builder"
echo "============================================"
echo "  Source:  $ROOT_DIR"
echo "  Staging: $STAGING"
echo ""

# ---- 1. Build ----
echo "[1/5] Building swordigo_boot..."
cd "$ROOT_DIR"
cmake -B build 2>&1 | tail -3
cmake --build build -j$(nproc) 2>&1 | tail -3
echo "      ✓ Built ($(du -h build/swordigo_boot | cut -f1))"

# ---- 2. Stage files ----
echo "[2/5] Staging package contents..."
rm -rf "$STAGING"
mkdir -p "$STAGING/usr/bin"
mkdir -p "$STAGING/usr/share/swordigo/assets/resources"
mkdir -p "$STAGING/usr/share/swordigo/src/assets"
mkdir -p "$STAGING/usr/share/swordigo/res/raw"
mkdir -p "$STAGING/usr/share/applications"
mkdir -p "$STAGING/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$STAGING/usr/share/licenses/${PKG_NAME}"

# Binary
cp build/swordigo_boot "$STAGING/usr/bin/"
chmod 755 "$STAGING/usr/bin/swordigo_boot"

# Game libraries (both versions — user picks in launcher)
for so in libswordigo.so libswordigo_nx.so; do
    if [ -f "$ROOT_DIR/$so" ]; then
        cp "$ROOT_DIR/$so" "$STAGING/usr/share/swordigo/"
        echo "      ✓ $so"
    fi
done

# Game assets (textures, models, scenes, etc.)
if [ -d "$ROOT_DIR/assets/resources" ]; then
    cp -r "$ROOT_DIR/assets/resources/"* "$STAGING/usr/share/swordigo/assets/resources/"
    ASSET_COUNT=$(find "$STAGING/usr/share/swordigo/assets/resources" -type f | wc -l)
    echo "      ✓ $ASSET_COUNT game asset files"
fi

# Music
if [ -d "$ROOT_DIR/res/raw" ]; then
    cp "$ROOT_DIR/res/raw/"*.mp3 "$STAGING/usr/share/swordigo/res/raw/" 2>/dev/null || true
    MUSIC_COUNT=$(find "$STAGING/usr/share/swordigo/res/raw" -type f | wc -l)
    echo "      ✓ $MUSIC_COUNT music files"
fi

# Launcher textures (icons, backgrounds, UI atlas)
cp "$ROOT_DIR/src/assets/"*.png "$STAGING/usr/share/swordigo/src/assets/" 2>/dev/null || true
echo "      ✓ Launcher assets"

# License
cp "$ROOT_DIR/LICENSE" "$STAGING/usr/share/licenses/${PKG_NAME}/" 2>/dev/null || true

# Desktop entry
cat > "$STAGING/usr/share/applications/Swordigo.desktop" << 'EOF'
[Desktop Entry]
Name=Swordigo Desktop
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop v3.0r — ARM Emulation Runtime
Keywords=swordigo;game;rpg;adventure;sword;
EOF

# Icon
if [ -f "$ROOT_DIR/src/assets/icon_gnome.png" ]; then
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$STAGING/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png"
fi

TOTAL_SIZE=$(du -sh "$STAGING" | cut -f1)
TOTAL_FILES=$(find "$STAGING" -type f | wc -l)
echo "      ✓ Staged $TOTAL_FILES files ($TOTAL_SIZE)"

# ---- 3. Generate RPM spec ----
echo "[3/5] Generating RPM spec..."

SPEC_DIR="/tmp/${PKG_NAME}-rpmbuild"
rm -rf "$SPEC_DIR"
mkdir -p "$SPEC_DIR"/{SPECS,BUILD,RPMS,SOURCES,SRPMS}

cat > "$SPEC_DIR/SPECS/${PKG_NAME}.spec" << SPEC
Name:           ${PKG_NAME}
Version:        ${VERSION}
Release:        ${RELEASE}%{?dist}
Summary:        Swordigo Desktop v3.0r — Native Linux Runtime
License:        MIT
Group:          Amusements/Games
URL:            https://github.com/TheCorrectSynovian/SwordigoDesktop

# Disable automatic dependency scanning (ships Android ARM .so files
# that confuse rpmbuild into requiring libandroid.so, libEGL.so, etc.)
AutoReq:        no
AutoProv:       no

%description
Swordigo Desktop v3.0r — A native Linux runtime for Swordigo.
ARM emulation via Unicorn Engine. Features: SDL3, advanced shaders
(SSAO, godrays, tone mapping), gamepad support, free camera, mod tools,
binary selector (v1.4.6 / v1.4.12), and more.

%install
cp -a ${STAGING}/* %{buildroot}/

%files
%{_bindir}/swordigo_boot
%{_datadir}/swordigo/
%{_datadir}/applications/Swordigo.desktop
%{_datadir}/icons/hicolor/256x256/apps/swordigo-desktop.png
%{_datadir}/licenses/${PKG_NAME}/

%changelog
* $(date +'%a %b %d %Y') QuantumCreeper <quantumcreeper@gmail.com> - ${VERSION}-${RELEASE}
- v3.0r Release: SDL3 migration, advanced shaders, dual binary support
SPEC

# ---- 4. Build RPM ----
echo "[4/5] Building RPM..."
rpmbuild -bb \
    --define "_topdir $SPEC_DIR" \
    --buildroot "$STAGING" \
    "$SPEC_DIR/SPECS/${PKG_NAME}.spec" 2>&1 | tail -5

# Find the output RPM
BUILT_RPM=$(find "$SPEC_DIR/RPMS" -name "*.rpm" -type f | head -1)
if [ -n "$BUILT_RPM" ]; then
    cp "$BUILT_RPM" "$RPM_OUT"
    echo ""
    echo "[5/5] ✅ RPM built successfully!"
    echo "  📦 $RPM_OUT"
    echo "  📏 $(du -h "$RPM_OUT" | cut -f1)"
    echo ""
    echo "Install:  sudo rpm -Uvh $RPM_OUT"
else
    echo "❌ RPM build failed"
    exit 1
fi

# Cleanup
rm -rf "$STAGING" "$SPEC_DIR"
