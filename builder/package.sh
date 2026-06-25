#!/bin/bash
set -e

# ============================================================
# Swordigo Desktop v7.0 — Vanilla Package Builder
# ============================================================
# Builds RPM and/or DEB with ALL runtime files included.
# No external downloads, no tar archives — everything is
# bundled directly into the package.
#
# Runtime data layout (Minecraft-style):
#   ~/.local/share/swordigo-desktop/
#     ├── assets/resources/     (game textures, models, scenes)
#     ├── engine/               (ARM binaries + libsre.so)
#     ├── launcher/             (launcher icons, textures, fonts)
#     ├── res/raw/              (music files)
#     ├── save/                 (user saves — NOT packaged)
#     └── manifest.json         (launcher instance registry)
#
# Package layout:
#   /usr/bin/swordigo_boot              (main binary)
#   /usr/bin/asset_viewer               (asset browser)
#   /usr/share/swordigo-desktop/        (bundled data)
#     ├── assets/resources/
#     ├── engine/v1.4.*/
#     ├── launcher/                (launcher icons, textures)
#     ├── res/raw/
#     └── manifest.json
#   /usr/share/applications/            (.desktop entry)
#   /usr/share/icons/                   (app icon)
#
# On install: post-install script copies from /usr/share/
# to the user's ~/.local/share/swordigo-desktop/ for full
# user access (like Minecraft launchers do).
#
# Usage:
#   ./package.sh            # Build RPM + DEB
#   ./package.sh rpm        # RPM only
#   ./package.sh deb        # DEB only
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="7.0.0"
RELEASE="1"
ARCH="x86_64"
BUILD_DIR="/tmp/swordigo-packaging"
PKG_NAME="swordigo-desktop"

# User's local runtime data — this is where assets/engine live
USER_DATA="$HOME/.local/share/swordigo-desktop"

# Only these engine versions are shipped
SHIP_VERSIONS=("v1.4.6" "v1.4.12")

FORMAT="${1:-all}"     # rpm | deb | all

echo "============================================"
echo " Swordigo Desktop v7.0 — Package Builder"
echo "============================================"
echo "  Format: $FORMAT"
echo "  Repo:   $ROOT_DIR"
echo "  Data:   $USER_DATA"
echo "  Shipped engines: ${SHIP_VERSIONS[*]}"
echo ""

# ============================================================
# Pre-flight: verify everything exists
# ============================================================
echo "[0/3] Pre-flight checks..."

# Binaries (built on ext4)
EXT4_DIR="$HOME/SwordigoDesktop"
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

# Assets
if [ ! -d "$USER_DATA/assets/resources" ]; then
    echo "  ❌ $USER_DATA/assets/resources/ not found!"
    echo "     Assets must be installed at ~/.local/share/swordigo-desktop/"
    exit 1
fi
ASSET_COUNT=$(find "$USER_DATA/assets/resources" -type f | wc -l)
echo "      ✓ $ASSET_COUNT game assets"

# Engine binaries
for ver in "${SHIP_VERSIONS[@]}"; do
    for arch in armeabi-v7a arm64-v8a; do
        so="$USER_DATA/engine/$ver/$arch/libswordigo.so"
        if [ -f "$so" ]; then
            echo "      ✓ engine/$ver/$arch/libswordigo.so"
        else
            echo "  ⚠ engine/$ver/$arch/libswordigo.so not found, skipping"
        fi
    done
done

# Music
MUSIC_COUNT=0
if [ -d "$USER_DATA/res/raw" ]; then
    MUSIC_COUNT=$(find "$USER_DATA/res/raw" -type f | wc -l)
    echo "      ✓ $MUSIC_COUNT music files"
fi

echo ""

# ============================================================
# Step 1: Stage — create the install tree
# ============================================================
stage_package() {
    local STAGING="$1"

    rm -rf "$STAGING"
    mkdir -p "$STAGING/usr/bin"
    mkdir -p "$STAGING/usr/share/$PKG_NAME"
    mkdir -p "$STAGING/usr/share/applications"
    mkdir -p "$STAGING/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$STAGING/usr/share/licenses/$PKG_NAME"

    echo "[1/3] Staging package files..."

    # ---- Binaries (from ext4 build) ----
    cp "$EXT4_DIR/swordigo_boot" "$STAGING/usr/bin/"
    chmod 755 "$STAGING/usr/bin/swordigo_boot"

    if [ -f "$EXT4_DIR/asset_viewer" ]; then
        cp "$EXT4_DIR/asset_viewer" "$STAGING/usr/bin/"
        chmod 755 "$STAGING/usr/bin/asset_viewer"
    fi

    # ---- libsre.so (bundled with engine data) ----
    # Will be placed alongside engine binaries during post-install
    cp "$EXT4_DIR/libsre.so" "$STAGING/usr/share/$PKG_NAME/"

    # ---- Engine binaries (from user's local data) ----
    for ver in "${SHIP_VERSIONS[@]}"; do
        for arch_dir in "$USER_DATA/engine/$ver"/armeabi-v7a "$USER_DATA/engine/$ver"/arm64-v8a; do
            [ -d "$arch_dir" ] || continue
            arch_name=$(basename "$arch_dir")
            dest="$STAGING/usr/share/$PKG_NAME/engine/$ver/$arch_name"
            mkdir -p "$dest"
            cp "$arch_dir"/*.so "$dest/" 2>/dev/null && \
                echo "      ✓ engine/$ver/$arch_name/ ($(ls "$arch_dir"/*.so | wc -l) .so files)"
        done
    done

    # Copy libsre.so into each ARM64 engine dir so it's ready
    for ver in "${SHIP_VERSIONS[@]}"; do
        arm64_dest="$STAGING/usr/share/$PKG_NAME/engine/$ver/arm64-v8a"
        if [ -d "$arm64_dest" ]; then
            cp "$EXT4_DIR/libsre.so" "$arm64_dest/"
        fi
    done
    echo "      ✓ libsre.so → all ARM64 engine dirs"

    # ---- Manifest ----
    cat > "$STAGING/usr/share/$PKG_NAME/engine/manifest.json" << 'MANIFEST'
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
    echo "      ✓ manifest.json (vanilla, 4 instances)"

    # ---- Game assets (from user's local data) ----
    echo "      Copying assets (this may take a moment)..."
    mkdir -p "$STAGING/usr/share/$PKG_NAME/assets"
    cp -r "$USER_DATA/assets/resources" "$STAGING/usr/share/$PKG_NAME/assets/"
    echo "      ✓ $(find "$STAGING/usr/share/$PKG_NAME/assets" -type f | wc -l) game assets"

    # ---- Music ----
    if [ -d "$USER_DATA/res/raw" ]; then
        mkdir -p "$STAGING/usr/share/$PKG_NAME/res/raw"
        cp "$USER_DATA/res/raw/"* "$STAGING/usr/share/$PKG_NAME/res/raw/" 2>/dev/null || true
        echo "      ✓ $(find "$STAGING/usr/share/$PKG_NAME/res/raw" -type f | wc -l) music files"
    fi

    # ---- Launcher assets (icons, textures, logos) ----
    # Stored in launcher/ subfolder for clean RPM/DEB access without permission issues
    mkdir -p "$STAGING/usr/share/$PKG_NAME/launcher"
    cp -r "$ROOT_DIR/src/assets/"* "$STAGING/usr/share/$PKG_NAME/launcher/" 2>/dev/null || true
    echo "      ✓ $(find "$STAGING/usr/share/$PKG_NAME/launcher" -type f | wc -l) launcher assets"

    # ---- Icon ----
    if [ -f "$ROOT_DIR/src/assets/icon_gnome.png" ]; then
        cp "$ROOT_DIR/src/assets/icon_gnome.png" "$STAGING/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png"
    fi

    # ---- License ----
    cp "$ROOT_DIR/LICENSE" "$STAGING/usr/share/licenses/$PKG_NAME/" 2>/dev/null || true

    # ---- Desktop entry ----
    cat > "$STAGING/usr/share/applications/Swordigo.desktop" << 'EOF'
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
EOF

    # ---- Post-install script ----
    # This script runs after RPM/DEB install to copy data to user's home.
    # For multi-user systems, each user gets their own copy on first launch.
    cat > "$STAGING/usr/bin/swordigo-setup" << 'SETUP'
#!/bin/bash
# swordigo-setup — Copy bundled data to user's local dir
# Runs automatically on first launch or manually by the user

DEST="$HOME/.local/share/swordigo-desktop"
SRC="/usr/share/swordigo-desktop"

if [ ! -d "$SRC" ]; then
    echo "[Setup] System data not found at $SRC"
    exit 1
fi

echo "[Setup] Installing Swordigo Desktop data to $DEST ..."
mkdir -p "$DEST"

# Copy engine (with libsre.so already inside)
if [ -d "$SRC/engine" ]; then
    cp -rn "$SRC/engine" "$DEST/" 2>/dev/null || cp -r "$SRC/engine" "$DEST/"
    echo "  ✓ Engine binaries"
fi

# Copy manifest
if [ -f "$SRC/engine/manifest.json" ]; then
    cp "$SRC/engine/manifest.json" "$DEST/engine/manifest.json"
    echo "  ✓ Manifest"
fi

# Copy assets
if [ -d "$SRC/assets" ]; then
    cp -rn "$SRC/assets" "$DEST/" 2>/dev/null || cp -r "$SRC/assets" "$DEST/"
    echo "  ✓ Game assets"
fi

# Copy music
if [ -d "$SRC/res" ]; then
    cp -rn "$SRC/res" "$DEST/" 2>/dev/null || cp -r "$SRC/res" "$DEST/"
    echo "  ✓ Music"
fi

# Copy launcher assets to dedicated launcher/ folder (no permission issues)
if [ -d "$SRC/launcher" ]; then
    mkdir -p "$DEST/launcher"
    cp -rn "$SRC/launcher/"* "$DEST/launcher/" 2>/dev/null || cp -r "$SRC/launcher/"* "$DEST/launcher/"
    echo "  ✓ Launcher assets"
fi

# Copy libsre.so to root (backup)
if [ -f "$SRC/libsre.so" ]; then
    cp "$SRC/libsre.so" "$DEST/libsre.so"
fi

echo ""
echo "[Setup] Done! Data installed to $DEST"
echo "        Run 'swordigo_boot' to play."
SETUP
    chmod 755 "$STAGING/usr/bin/swordigo-setup"

    local TOTAL_FILES=$(find "$STAGING" -type f | wc -l)
    local TOTAL_SIZE=$(du -sh "$STAGING" | cut -f1)
    echo ""
    echo "      ✓ Staged $TOTAL_FILES files ($TOTAL_SIZE)"
}

# ============================================================
# Step 2a: RPM builder
# ============================================================
build_rpm() {
    local STAGING="${BUILD_DIR}/swordigo-rpm-root"
    local RPM_TOPDIR="${BUILD_DIR}/swordigo-rpmbuild"
    local RPM_OUT="${BUILD_DIR}/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"

    echo ""
    echo "── [2/3] Building RPM ──"
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
Summary:        Swordigo Desktop v7.0 — Native Linux runtime with Dynarmic JIT
License:        MIT
Group:          Amusements/Games
URL:            https://github.com/TheCorrectSynovian/SwordigoDesktop
AutoReq:        no
AutoProv:       no

%description
Swordigo Desktop v7.0 — Native Linux runtime for Swordigo.
Dynarmic JIT compiler (default) for 60fps ARM64 emulation.
Unicorn Engine fallback for maximum compatibility.
SDL3, OpenGL/Vulkan, OpenAL. RLSwordigo + KiwiAPI mod support.
34+ SRE hooks, ImGui launcher, Lua scripting API (Mini/LNI).
PostFX pipeline, save editor, asset viewer, gamepad support.
Includes all game assets, engine binaries, and music.

%install
cp -a ${BUILDROOT}/* %{buildroot}/

%post
# Post-install: run setup for the installing user
if [ -x /usr/bin/swordigo-setup ]; then
    su "\${SUDO_USER:-\$USER}" -c /usr/bin/swordigo-setup || true
fi

%files
/usr/bin/swordigo_boot
/usr/bin/swordigo-setup
# asset_viewer is optional — only packaged if built
/usr/share/${PKG_NAME}/
/usr/share/applications/Swordigo.desktop
/usr/share/icons/hicolor/256x256/apps/swordigo-desktop.png
/usr/share/licenses/${PKG_NAME}/

%changelog
* $(date +'%a %b %d %Y') QuantumCreeper <quantumcreeper@gmail.com> - ${VERSION}-${RELEASE}
- v7.0 Release — Dynarmic JIT: The Performance Revolution
- NEW: Dynarmic JIT compiler — ARM64 at near-native speed (60fps)
- NEW: RLSwordigo support — play the roguelike spinoff
- NEW: KiwiAPI / Combatch mod compatibility layer
- NEW: Bauble API + Achievement System hooks
- PERF: 10-15fps (Unicorn) → 60fps locked (Dynarmic)
- Unicorn Engine retained as --no-dynarmic fallback
- 34+ SRE hooks, ImGui launcher, PostFX pipeline
- All assets, engine binaries, and music bundled
SPEC

    echo ""
    echo "[3/3] Building RPM (this may take a minute)..."
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
# Step 2b: DEB builder
# ============================================================
build_deb() {
    local STAGING="${BUILD_DIR}/swordigo-deb-root"
    local DEB_OUT="${BUILD_DIR}/${PKG_NAME}_${VERSION}-${RELEASE}_amd64.deb"

    echo ""
    echo "── [2/3] Building DEB ──"
    stage_package "$STAGING"

    mkdir -p "$STAGING/DEBIAN"
    cat > "$STAGING/DEBIAN/control" << CTRL
Package: ${PKG_NAME}
Version: ${VERSION}-${RELEASE}
Section: games
Priority: optional
Architecture: amd64
Maintainer: QuantumCreeper <quantumcreeper@gmail.com>
Description: Swordigo Desktop v7.0 — Native Linux runtime with Dynarmic JIT
 Complete Swordigo Desktop with all game assets, engine binaries,
 music, and tools. Installs to ~/.local/share/swordigo-desktop/
 for full user access (Minecraft-style data management).
 .
 v7.0: Dynarmic JIT for 60fps ARM64 emulation, RLSwordigo support,
 KiwiAPI mod compatibility, 34+ SRE hooks, PostFX, save editor.
Homepage: https://github.com/TheCorrectSynovian/SwordigoDesktop
CTRL

    # DEB post-install script
    cat > "$STAGING/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
# Copy game data to the installing user's home directory
if [ -x /usr/bin/swordigo-setup ]; then
    su "${SUDO_USER:-$USER}" -c /usr/bin/swordigo-setup || true
fi
POSTINST
    chmod 755 "$STAGING/DEBIAN/postinst"

    echo ""
    echo "[3/3] Building DEB (this may take a minute)..."
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
echo ""
echo "After install, users can also run: swordigo-setup"
echo "to re-copy game data to their home directory."
