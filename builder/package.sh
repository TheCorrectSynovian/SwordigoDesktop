#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop v3.0r — Universal Package Builder
# Builds RPM and/or DEB for Swordigo and/or RLSwordigo
#
# Usage:
#   ./package.sh                    # Build all (swordigo rpm+deb, rl rpm+deb)
#   ./package.sh swordigo rpm       # Swordigo RPM only
#   ./package.sh rlswordigo deb     # RLSwordigo DEB only
#   ./package.sh swordigo all       # Swordigo RPM + DEB
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="3.0.0"
RELEASE="1"
ARCH="x86_64"
BUILD_DIR="/tmp/swordigo-packaging"

# Parse args
GAME="${1:-all}"       # swordigo | rlswordigo | all
FORMAT="${2:-all}"     # rpm | deb | all

echo "============================================"
echo " Swordigo Desktop v3.0r — Package Builder"
echo "============================================"
echo "  Game:   $GAME"
echo "  Format: $FORMAT"
echo "  Source:  $ROOT_DIR"
echo ""

# ============================================================
# Step 1: Build the binary (shared by all packages)
# ============================================================
echo "[1/4] Building swordigo_boot..."
cd "$ROOT_DIR"
cmake -B build 2>&1 | tail -1
cmake --build build -j$(nproc) 2>&1 | tail -1
echo "      ✓ Built ($(du -h build/swordigo_boot | cut -f1))"

# ============================================================
# Step 2: Stage function — creates the file tree for a variant
# ============================================================
stage_package() {
    local VARIANT="$1"    # "swordigo" or "rlswordigo"
    local STAGING="$2"

    rm -rf "$STAGING"
    mkdir -p "$STAGING/usr/bin"
    mkdir -p "$STAGING/usr/share/swordigo/src/assets"
    mkdir -p "$STAGING/usr/share/applications"
    mkdir -p "$STAGING/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$STAGING/usr/share/licenses/swordigo-desktop"

    # Binary (same for both)
    cp "$ROOT_DIR/build/swordigo_boot" "$STAGING/usr/bin/"
    chmod 755 "$STAGING/usr/bin/swordigo_boot"

    # Launcher textures (same for both)
    cp "$ROOT_DIR/src/assets/"*.png "$STAGING/usr/share/swordigo/src/assets/" 2>/dev/null || true

    # Icon + desktop + license (same for both)
    cp "$ROOT_DIR/src/assets/icon_gnome.png" "$STAGING/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png" 2>/dev/null || true
    cp "$ROOT_DIR/LICENSE" "$STAGING/usr/share/licenses/swordigo-desktop/" 2>/dev/null || true

    if [ "$VARIANT" = "swordigo" ]; then
        # ---- VANILLA SWORDIGO ----
        # Game binaries (both .so versions for user choice)
        for so in libswordigo.so libswordigo_nx.so; do
            [ -f "$ROOT_DIR/$so" ] && cp "$ROOT_DIR/$so" "$STAGING/usr/share/swordigo/" && echo "      ✓ $so"
        done

        # Game assets
        mkdir -p "$STAGING/usr/share/swordigo/assets/resources"
        cp -r "$ROOT_DIR/assets/resources/"* "$STAGING/usr/share/swordigo/assets/resources/"
        echo "      ✓ $(find "$STAGING/usr/share/swordigo/assets/resources" -type f | wc -l) game assets"

        # Music
        mkdir -p "$STAGING/usr/share/swordigo/res/raw"
        cp "$ROOT_DIR/res/raw/"*.mp3 "$STAGING/usr/share/swordigo/res/raw/" 2>/dev/null || true
        echo "      ✓ $(find "$STAGING/usr/share/swordigo/res/raw" -type f | wc -l) music files"

        cat > "$STAGING/usr/share/applications/Swordigo.desktop" << 'EOF'
[Desktop Entry]
Name=Swordigo Desktop
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop v3.0r
Keywords=swordigo;game;rpg;adventure;
StartupWMClass=Swordigo
EOF

    elif [ "$VARIANT" = "rlswordigo" ]; then
        # ---- RL SWORDIGO ----
        # RL game binaries + dependencies
        for so in rl_libswordigo.so rl_libmini.so rl_libGlossHook.so; do
            [ -f "$ROOT_DIR/$so" ] && cp "$ROOT_DIR/$so" "$STAGING/usr/share/swordigo/" && echo "      ✓ $so"
        done
        # Also ship vanilla binaries so user can switch
        for so in libswordigo.so libswordigo_nx.so; do
            [ -f "$ROOT_DIR/$so" ] && cp "$ROOT_DIR/$so" "$STAGING/usr/share/swordigo/" && echo "      ✓ $so"
        done

        # RL assets
        mkdir -p "$STAGING/usr/share/swordigo/rl_assets"
        cp -r "$ROOT_DIR/rl_assets/"* "$STAGING/usr/share/swordigo/rl_assets/"
        echo "      ✓ $(find "$STAGING/usr/share/swordigo/rl_assets" -type f | wc -l) RL assets"

        # Also need vanilla assets (base game)
        mkdir -p "$STAGING/usr/share/swordigo/assets/resources"
        cp -r "$ROOT_DIR/assets/resources/"* "$STAGING/usr/share/swordigo/assets/resources/"
        echo "      ✓ $(find "$STAGING/usr/share/swordigo/assets/resources" -type f | wc -l) base assets"

        # Music (both vanilla and RL)
        mkdir -p "$STAGING/usr/share/swordigo/res/raw"
        cp "$ROOT_DIR/res/raw/"*.mp3 "$STAGING/usr/share/swordigo/res/raw/" 2>/dev/null || true
        if [ -d "$ROOT_DIR/rl_assets/music" ]; then
            cp "$ROOT_DIR/rl_assets/music/"* "$STAGING/usr/share/swordigo/rl_assets/music/" 2>/dev/null || true
        fi
        echo "      ✓ Music files staged"

        cat > "$STAGING/usr/share/applications/Swordigo.desktop" << 'EOF'
[Desktop Entry]
Name=Swordigo Desktop (RL Edition)
Exec=swordigo_boot
Icon=swordigo-desktop
Terminal=false
Type=Application
Categories=Game;ActionGame;AdventureGame;
Comment=Swordigo Desktop v3.0r — RLSwordigo Edition
Keywords=swordigo;rlswordigo;game;rpg;mod;
StartupWMClass=Swordigo
EOF
    fi

    local TOTAL_FILES=$(find "$STAGING" -type f | wc -l)
    local TOTAL_SIZE=$(du -sh "$STAGING" | cut -f1)
    echo "      ✓ Staged $TOTAL_FILES files ($TOTAL_SIZE)"
}

# ============================================================
# Step 3: RPM builder
# ============================================================
build_rpm() {
    local VARIANT="$1"
    local PKG_NAME="swordigo-desktop"
    [ "$VARIANT" = "rlswordigo" ] && PKG_NAME="swordigo-desktop-rl"

    local STAGING="${BUILD_DIR}/${VARIANT}-rpm-root"
    local RPM_TOPDIR="${BUILD_DIR}/${VARIANT}-rpmbuild"
    local RPM_OUT="${BUILD_DIR}/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"

    echo ""
    echo "── Building RPM: $PKG_NAME ──"
    stage_package "$VARIANT" "$STAGING"

    rm -rf "$RPM_TOPDIR"
    mkdir -p "$RPM_TOPDIR"/{SPECS,BUILD,RPMS,SOURCES,SRPMS,BUILDROOT}

    # Copy staged files into rpmbuild's BUILDROOT
    local BUILDROOT="$RPM_TOPDIR/BUILDROOT/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}"
    mkdir -p "$BUILDROOT"
    cp -a "$STAGING/"* "$BUILDROOT/"

    cat > "$RPM_TOPDIR/SPECS/${PKG_NAME}.spec" << SPEC
Name:           ${PKG_NAME}
Version:        ${VERSION}
Release:        ${RELEASE}
Summary:        Swordigo Desktop v3.0r
License:        MIT
Group:          Amusements/Games
URL:            https://github.com/TheCorrectSynovian/SwordigoDesktop
AutoReq:        no
AutoProv:       no

%description
Swordigo Desktop v3.0r — Native Linux runtime for Swordigo.
ARM emulation via Unicorn Engine. SDL3, HiDPI native-resolution rendering,
advanced shaders (SSAO, godrays, tone mapping), dynamic render resolution,
binary selector (v1.4.6 / v1.4.12 / RL), gamepad support, mod tools.

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
- v3.0r Release
- HiDPI: Full native resolution rendering on high-DPI displays
- HiDPI: Dynamic render resolution matched to physical drawable pixels
- HiDPI: Automatic aspect ratio detection (16:9, 16:10, 3:2, etc)
- HiDPI: Supersampling AA when rendering above native drawable size
- HiDPI: Separated logical/physical coordinate spaces for GL and input
- SDL3: Complete migration from SDL2 to SDL3 API
- SDL3: SDL_WINDOW_HIGH_PIXEL_DENSITY, SDL_GetWindowSizeInPixels
- Binary: libswordigo_nx.so (v1.4.12) now default, marked TESTED
- Binary: RL Swordigo support with rl_libswordigo.so (v6.1-rl)
- Fix: Death screen hang — patched ShowInterstitialAd + auto-restart
- Fix: Blurry rendering on HiDPI displays
- Fix: Window overflow on scaled desktops
- Fix: FBO quarter-screen render artifact
- Fix: Fullscreen toggle physical pixel handling
- Engine: Dynamic touch coordinate scaling
- Engine: Render resolution clamping (1920-4096 width)
- Debug: F3 overlay shows render, window, and drawable dimensions
- Packaging: Standalone RPM+DEB builder (no CMake/CPack)
SPEC

    rpmbuild -bb \
        --define "_topdir $RPM_TOPDIR" \
        "$RPM_TOPDIR/SPECS/${PKG_NAME}.spec" 2>&1 | tail -3

    local BUILT=$(find "$RPM_TOPDIR/RPMS" -name "*.rpm" -type f | head -1)
    if [ -n "$BUILT" ]; then
        cp "$BUILT" "$RPM_OUT"
        echo "  ✅ $RPM_OUT ($(du -h "$RPM_OUT" | cut -f1))"
    else
        echo "  ❌ RPM build failed for $VARIANT"
    fi

    rm -rf "$STAGING" "$RPM_TOPDIR"
}

# ============================================================
# Step 4: DEB builder
# ============================================================
build_deb() {
    local VARIANT="$1"
    local PKG_NAME="swordigo-desktop"
    [ "$VARIANT" = "rlswordigo" ] && PKG_NAME="swordigo-desktop-rl"

    local STAGING="${BUILD_DIR}/${VARIANT}-deb-root"
    local DEB_OUT="${BUILD_DIR}/${PKG_NAME}_${VERSION}-${RELEASE}_amd64.deb"

    echo ""
    echo "── Building DEB: $PKG_NAME ──"
    stage_package "$VARIANT" "$STAGING"

    # DEBIAN control
    mkdir -p "$STAGING/DEBIAN"
    cat > "$STAGING/DEBIAN/control" << CTRL
Package: ${PKG_NAME}
Version: ${VERSION}-${RELEASE}
Section: games
Priority: optional
Architecture: amd64
Maintainer: QuantumCreeper <quantumcreeper@gmail.com>
Description: Swordigo Desktop v3.0r
 Native Linux runtime for Swordigo. ARM emulation via Unicorn Engine.
 SDL3, HiDPI native-resolution rendering, advanced shaders (SSAO, godrays,
 tone mapping), dynamic render resolution matching display physical pixels,
 binary selector (v1.4.6 / v1.4.12 / RL), gamepad support, mod tools.
 .
 Changes in v3.0r:
 - HiDPI: Full native resolution rendering on high-DPI displays
 - HiDPI: Dynamic render resolution and aspect ratio detection
 - SDL3: Complete migration from SDL2 to SDL3
 - Binary: libswordigo_nx.so (v1.4.12) now default
 - Binary: RL Swordigo mod support
 - Fix: Death screen hang, blurry HiDPI, window overflow
 - Standalone RPM+DEB packaging
Homepage: https://github.com/TheCorrectSynovian/SwordigoDesktop
CTRL

    dpkg-deb --build "$STAGING" "$DEB_OUT" 2>&1 | tail -1
    if [ -f "$DEB_OUT" ]; then
        echo "  ✅ $DEB_OUT ($(du -h "$DEB_OUT" | cut -f1))"
    else
        echo "  ❌ DEB build failed for $VARIANT"
    fi

    rm -rf "$STAGING"
}

# ============================================================
# Run
# ============================================================
mkdir -p "$BUILD_DIR"

run_variant() {
    local V="$1"
    if [ "$FORMAT" = "rpm" ] || [ "$FORMAT" = "all" ]; then
        build_rpm "$V"
    fi
    if [ "$FORMAT" = "deb" ] || [ "$FORMAT" = "all" ]; then
        if command -v dpkg-deb &>/dev/null; then
            build_deb "$V"
        else
            echo "  ⚠ dpkg-deb not found — skipping DEB (install dpkg on Fedora: sudo dnf install dpkg)"
        fi
    fi
}

if [ "$GAME" = "swordigo" ] || [ "$GAME" = "all" ]; then
    run_variant "swordigo"
fi
if [ "$GAME" = "rlswordigo" ] || [ "$GAME" = "all" ]; then
    run_variant "rlswordigo"
fi

echo ""
echo "============================================"
echo " All packages:"
ls -lh "$BUILD_DIR/"*.rpm "$BUILD_DIR/"*.deb 2>/dev/null
echo "============================================"
echo ""
echo "Install RPM:  sudo rpm -Uvh <file>.rpm"
echo "Install DEB:  sudo dpkg -i <file>.deb"
