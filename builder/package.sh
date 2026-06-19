#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop v4.5r — Vanilla Package Builder
# Builds RPM and/or DEB — vanilla only (v1.4.6 + v1.4.12)
#
# Usage:
#   ./package.sh            # Build RPM + DEB
#   ./package.sh rpm        # RPM only
#   ./package.sh deb        # DEB only
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="4.5.0"
RELEASE="1"
ARCH="x86_64"
BUILD_DIR="/tmp/swordigo-packaging"
PKG_NAME="swordigo-desktop"

# Only these engine versions are shipped
SHIP_VERSIONS=("v1.4.6" "v1.4.12")

FORMAT="${1:-all}"     # rpm | deb | all

echo "============================================"
echo " Swordigo Desktop v4.5r — Package Builder"
echo "============================================"
echo "  Format: $FORMAT"
echo "  Source:  $ROOT_DIR"
echo "  Shipped engines: ${SHIP_VERSIONS[*]}"
echo ""

# ============================================================
# Step 1: Build the binary via Makefile (NO cmake)
# ============================================================
echo "[1/3] Building swordigo_boot..."
cd "$ROOT_DIR"
make -j$(nproc) 2>&1 | tail -3
if [ ! -f "$ROOT_DIR/swordigo_boot" ]; then
    echo "  ❌ Build failed — swordigo_boot not found"
    exit 1
fi
echo "      ✓ Built ($(du -h swordigo_boot | cut -f1))"

# ============================================================
# Step 2: Stage — create the install tree
# ============================================================
stage_package() {
    local STAGING="$1"

    rm -rf "$STAGING"
    mkdir -p "$STAGING/usr/bin"
    mkdir -p "$STAGING/usr/share/swordigo/src/assets"
    mkdir -p "$STAGING/usr/share/applications"
    mkdir -p "$STAGING/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$STAGING/usr/share/licenses/swordigo-desktop"

    # Binary
    cp "$ROOT_DIR/swordigo_boot" "$STAGING/usr/bin/"
    chmod 755 "$STAGING/usr/bin/swordigo_boot"

    # Launcher textures (including icons/ subdirectory)
    cp -r "$ROOT_DIR/src/assets/"* "$STAGING/usr/share/swordigo/src/assets/" 2>/dev/null || true

    # Icon + license
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$STAGING/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png" 2>/dev/null || true
    cp "$ROOT_DIR/LICENSE" "$STAGING/usr/share/licenses/swordigo-desktop/" 2>/dev/null || true

    # ---- Engine: ONLY v1.4.6 and v1.4.12 ----
    mkdir -p "$STAGING/usr/share/swordigo/engine"
    for ver in "${SHIP_VERSIONS[@]}"; do
        ver_dir="$ROOT_DIR/engine/$ver"
        [ -d "$ver_dir" ] || { echo "  ⚠ engine/$ver not found, skipping"; continue; }
        for arch_dir in "$ver_dir"/armeabi-v7a "$ver_dir"/arm64-v8a; do
            [ -d "$arch_dir" ] || continue
            arch_name=$(basename "$arch_dir")
            dest="$STAGING/usr/share/swordigo/engine/$ver/$arch_name"
            mkdir -p "$dest"
            cp "$arch_dir"/*.so "$dest/" 2>/dev/null && \
                echo "      ✓ engine/$ver/$arch_name/ ($(ls "$arch_dir"/*.so | wc -l) .so files)"
        done
    done

    # ---- Manifest: filter to only shipped versions ----
    # Generate a clean manifest with only vanilla v1.4.6 and v1.4.12
    cat > "$STAGING/usr/share/swordigo/engine/manifest.json" << 'MANIFEST'
{
  "default": "engine/v1.4.12/armeabi-v7a/libswordigo.so",
  "instances": [
    {
      "name": "v1.4.12 [ARM64] (Tested)",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.12/arm64-v8a/libswordigo.so",
      "sha256": "f847814d1b6f81268567ed5ec2473fea4d4ee3b75d2c6fec7057227225e989f8",
      "version": "1.4.12",
      "version_dir": "v1.4.12",
      "arch": "ARM64",
      "file_size": 7230152,
      "is_default": false,
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
      "is_default": true,
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
    echo "      ✓ manifest.json (vanilla only, 4 instances)"

    # ---- Game assets (vanilla only, NO rl_assets) ----
    mkdir -p "$STAGING/usr/share/swordigo/assets/resources"
    cp -r "$ROOT_DIR/assets/resources/"* "$STAGING/usr/share/swordigo/assets/resources/"
    echo "      ✓ $(find "$STAGING/usr/share/swordigo/assets/resources" -type f | wc -l) game assets"

    # Music
    mkdir -p "$STAGING/usr/share/swordigo/res/raw"
    cp "$ROOT_DIR/res/raw/"*.mp3 "$STAGING/usr/share/swordigo/res/raw/" 2>/dev/null || true
    echo "      ✓ $(find "$STAGING/usr/share/swordigo/res/raw" -type f | wc -l) music files"

    # Desktop entry
    cat > "$STAGING/usr/share/applications/Swordigo.desktop" << 'EOF'
[Desktop Entry]
Name=Swordigo Desktop
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop v4.5r
Keywords=swordigo;game;rpg;adventure;
StartupWMClass=Swordigo
EOF

    local TOTAL_FILES=$(find "$STAGING" -type f | wc -l)
    local TOTAL_SIZE=$(du -sh "$STAGING" | cut -f1)
    echo "      ✓ Staged $TOTAL_FILES files ($TOTAL_SIZE)"
}

# ============================================================
# Step 3a: RPM builder
# ============================================================
build_rpm() {
    local STAGING="${BUILD_DIR}/swordigo-rpm-root"
    local RPM_TOPDIR="${BUILD_DIR}/swordigo-rpmbuild"
    local RPM_OUT="${BUILD_DIR}/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"

    echo ""
    echo "── Building RPM: $PKG_NAME ──"
    stage_package "$STAGING"

    rm -rf "$RPM_TOPDIR"
    mkdir -p "$RPM_TOPDIR"/{SPECS,BUILD,RPMS,SOURCES,SRPMS,BUILDROOT}

    local BUILDROOT="$RPM_TOPDIR/BUILDROOT/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}"
    mkdir -p "$BUILDROOT"
    cp -a "$STAGING/"* "$BUILDROOT/"

    cat > "$RPM_TOPDIR/SPECS/${PKG_NAME}.spec" << SPEC
Name:           ${PKG_NAME}
Version:        ${VERSION}
Release:        ${RELEASE}
Summary:        Swordigo Desktop v4.5r
License:        MIT
Group:          Amusements/Games
URL:            https://github.com/TheCorrectSynovian/SwordigoDesktop
AutoReq:        no
AutoProv:       no

%description
Swordigo Desktop v4.5r — Native Linux runtime for Swordigo.
ARM emulation via Unicorn Engine. SDL3, HiDPI rendering,
PostFX (SSAO, god rays, bloom, FSR upscaling), draw call batcher,
threading bridges, binary/mod selector, gamepad support.

%install
cp -a ${BUILDROOT}/* %{buildroot}/

%files
/usr/bin/swordigo_boot
/usr/share/swordigo/
/usr/share/applications/Swordigo.desktop
/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png
/usr/share/licenses/swordigo-desktop/

%changelog
* $(date +'%a %b %d %Y') QuantumCreeper <quantumcreeper@gmail.com> - ${VERSION}-${RELEASE}
- v4.5r Release
- Save Editor: Revamped, moved to launcher window
- Save Editor: Browse and edit .gplayer saves (coins, health, mana, XP, weapon, keys)
- ARM64: Entity loop infinite loop fix (NOP at 0x580708)
- GPU: Draw call batcher (80-140 draws -> ~20 per frame)
- GPU: FSR 1.0 edge-adaptive spatial upscaling
- GPU: GLSL #version 330 shaders
- Threading: Real nanosleep/usleep/pthread_join/pthread_detach bridges
- PostFX: SSAO, god rays, bloom, vignette, chromatic aberration
- Launcher: PolyMC-style instance manager with detail panel
- ARM64: Full ARM64 emulation via Unicorn Engine
- HiDPI: Native resolution rendering on high-DPI displays
- SDL3: Complete SDL3 migration
SPEC

    rpmbuild -bb \
        --define "_topdir $RPM_TOPDIR" \
        "$RPM_TOPDIR/SPECS/${PKG_NAME}.spec" 2>&1 | tail -3

    local BUILT=$(find "$RPM_TOPDIR/RPMS" -name "*.rpm" -type f | head -1)
    if [ -n "$BUILT" ]; then
        cp "$BUILT" "$RPM_OUT"
        echo "  ✅ $RPM_OUT ($(du -h "$RPM_OUT" | cut -f1))"
    else
        echo "  ❌ RPM build failed"
    fi

    rm -rf "$STAGING" "$RPM_TOPDIR"
}

# ============================================================
# Step 3b: DEB builder
# ============================================================
build_deb() {
    local STAGING="${BUILD_DIR}/swordigo-deb-root"
    local DEB_OUT="${BUILD_DIR}/${PKG_NAME}_${VERSION}-${RELEASE}_amd64.deb"

    echo ""
    echo "── Building DEB: $PKG_NAME ──"
    stage_package "$STAGING"

    mkdir -p "$STAGING/DEBIAN"
    cat > "$STAGING/DEBIAN/control" << CTRL
Package: ${PKG_NAME}
Version: ${VERSION}-${RELEASE}
Section: games
Priority: optional
Architecture: amd64
Maintainer: QuantumCreeper <quantumcreeper@gmail.com>
Description: Swordigo Desktop v4.5r
 Native Linux runtime for Swordigo. ARM emulation via Unicorn Engine.
 SDL3, HiDPI rendering, PostFX (SSAO, god rays, bloom, FSR upscaling),
 GPU draw call batcher, save editor, binary/mod selector, gamepad.
 .
 Changes in v4.5r:
 - Save Editor: Revamped, moved from in-game to launcher window
 - Save Editor: Browse/edit .gplayer saves (coins, health, mana, XP, weapon, keys)
 - ARM64: Entity loop infinite loop fix
 - Version bump to v4.5r
Homepage: https://github.com/TheCorrectSynovian/SwordigoDesktop
CTRL

    dpkg-deb --build "$STAGING" "$DEB_OUT" 2>&1 | tail -1
    if [ -f "$DEB_OUT" ]; then
        echo "  ✅ $DEB_OUT ($(du -h "$DEB_OUT" | cut -f1))"
    else
        echo "  ❌ DEB build failed"
    fi

    rm -rf "$STAGING"
}

# ============================================================
# Run
# ============================================================
mkdir -p "$BUILD_DIR"

if [ "$FORMAT" = "rpm" ] || [ "$FORMAT" = "all" ]; then
    build_rpm
fi
if [ "$FORMAT" = "deb" ] || [ "$FORMAT" = "all" ]; then
    if command -v dpkg-deb &>/dev/null; then
        build_deb
    else
        echo "  ⚠ dpkg-deb not found — skipping DEB (install: sudo dnf install dpkg)"
    fi
fi

echo ""
echo "============================================"
echo " Packages built:"
ls -lh "$BUILD_DIR/"*.rpm "$BUILD_DIR/"*.deb 2>/dev/null
echo "============================================"
echo ""
echo "Install RPM:  sudo rpm -Uvh <file>.rpm"
echo "Install DEB:  sudo dpkg -i <file>.deb"
