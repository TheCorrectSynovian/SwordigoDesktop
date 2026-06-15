#!/usr/bin/env bash
# =============================================================================
# instx.sh — SwordigoDesktop asset & library installer
# =============================================================================
#
# Downloads and installs the required proprietary game assets and native
# library so the project can be built and run.
#
# Usage:
#   ./instx.sh
#
# On first run, creates an installres/ temp directory, downloads the files,
# extracts assets, places libswordigo.so, and cleans up.
#
# On re-run, prompts before overwriting existing files.
#
# Dependencies: bash, curl or wget, tar
# =============================================================================

set -euo pipefail

# ---- Configuration ----------------------------------------------------------

# Google Drive file IDs
ASSETS_FILE_ID="1sCKg-CM2oNur7E50gGT7LzCO353JEQCr"
LIBRARY_FILE_ID="1ihW2hlL0eApMvokmZZaKu4gh1M1fiqT5"

# Download URLs (Google Drive direct-download format)
ASSETS_URL="https://drive.google.com/uc?export=download&id=${ASSETS_FILE_ID}"
LIBRARY_URL="https://drive.google.com/uc?export=download&id=${LIBRARY_FILE_ID}"

# Destination paths (relative to script location)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ASSETS_DIR="${SCRIPT_DIR}/assets"
LIBRARY_PATH="${SCRIPT_DIR}/libswordigo.so"
TEMP_DIR="${SCRIPT_DIR}/installres"

# Filenames inside the temp directory
ASSETS_ARCHIVE="assets.tar.xz"
LIBRARY_FILE="libswordigo.so"

# ---- Helper functions -------------------------------------------------------

print_info() {
    echo "  [INFO] $*"
}

print_ok() {
    echo "  [ OK ] $*"
}

print_warn() {
    echo "  [WARN] $*"
}

print_error() {
    echo "  [FAIL] $*" >&2
}

# Detect download tool: prefer curl, fall back to wget
detect_downloader() {
    if command -v curl &>/dev/null; then
        DOWNLOADER="curl"
    elif command -v wget &>/dev/null; then
        DOWNLOADER="wget"
    else
        print_error "Neither 'curl' nor 'wget' found. Please install one and try again."
        exit 1
    fi
    print_info "Using download tool: ${DOWNLOADER}"
}

# Download a file from a URL to a destination path with progress display
download_file() {
    local url="$1"
    local dest="$2"
    local label="$3"

    print_info "Downloading ${label}..."

    case "${DOWNLOADER}" in
        curl)
            curl -L -o "${dest}" -# "${url}"
            ;;
        wget)
            wget --show-progress -O "${dest}" "${url}"
            ;;
    esac

    if [[ ! -f "${dest}" ]]; then
        print_error "Download failed: ${label}"
        exit 1
    fi

    print_ok "Downloaded ${label}"
}

# Prompt the user with a yes/no question; returns true for yes
prompt_overwrite() {
    local path="$1"
    local label="$2"

    echo ""
    echo "  ${label} already exists at:"
    echo "    ${path}"
    read -r -p "  Overwrite? [y/N] " response
    case "${response}" in
        [yY]|[yY][eE][sS])
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}


# ---- Main -------------------------------------------------------------------

main() {
    echo ""
    echo "  ╔══════════════════════════════════════════════╗"
    echo "  ║   SwordigoDesktop Installer                  ║"
    echo "  ║   Downloads proprietary assets & library     ║"
    echo "  ╚══════════════════════════════════════════════╝"
    echo ""

    # Detect which download tool is available
    detect_downloader

    # Create temporary working directory
    mkdir -p "${TEMP_DIR}"
    print_info "Created temporary directory: ${TEMP_DIR}"

    # ---- Handle existing libswordigo.so --------------------------------------
    if [[ -f "${LIBRARY_PATH}" ]]; then
        if prompt_overwrite "${LIBRARY_PATH}" "libswordigo.so"; then
            rm -f "${LIBRARY_PATH}"
            download_file "${LIBRARY_URL}" "${TEMP_DIR}/${LIBRARY_FILE}" "shared library"
            cp "${TEMP_DIR}/${LIBRARY_FILE}" "${LIBRARY_PATH}"
            print_ok "Placed libswordigo.so"
        else
            print_info "Skipping libswordigo.so — using existing file."
        fi
    else
        download_file "${LIBRARY_URL}" "${TEMP_DIR}/${LIBRARY_FILE}" "shared library"
        cp "${TEMP_DIR}/${LIBRARY_FILE}" "${LIBRARY_PATH}"
        print_ok "Placed libswordigo.so"
    fi

    # ---- Handle existing assets directory ------------------------------------
    DO_EXTRACT=false
    if [[ -d "${ASSETS_DIR}" && -n "$(ls -A "${ASSETS_DIR}" 2>/dev/null)" ]]; then
        if prompt_overwrite "${ASSETS_DIR}" "Assets directory"; then
            rm -rf "${ASSETS_DIR}"
            DO_EXTRACT=true
        else
            print_info "Skipping assets — using existing directory."
        fi
    else
        DO_EXTRACT=true
    fi

    if [[ "${DO_EXTRACT}" == true ]]; then
        download_file "${ASSETS_URL}" "${TEMP_DIR}/${ASSETS_ARCHIVE}" "assets archive"
        print_info "Extracting assets..."
        mkdir -p "${ASSETS_DIR}"
        tar -xJf "${TEMP_DIR}/${ASSETS_ARCHIVE}" -C "${ASSETS_DIR}"
        print_ok "Extracted assets to ${ASSETS_DIR}"
    fi


    # ---- Verification --------------------------------------------------------
    echo ""
    print_info "Verifying installation..."

    local errors=0

    if [[ ! -f "${LIBRARY_PATH}" ]]; then
        print_error "Missing: ${LIBRARY_PATH}"
        errors=$((errors + 1))
    else
        print_ok "Found libswordigo.so"
    fi

    if [[ ! -d "${ASSETS_DIR}" ]]; then
        print_error "Missing: ${ASSETS_DIR}"
        errors=$((errors + 1))
    else
        print_ok "Found assets directory"
    fi

    # ---- Cleanup -------------------------------------------------------------
    print_info "Cleaning up temporary files..."
    rm -rf "${TEMP_DIR}"
    print_ok "Removed temporary directory: ${TEMP_DIR}"

    # ---- Summary -------------------------------------------------------------
    echo ""
    if [[ "${errors}" -eq 0 ]]; then
        echo "  ╔══════════════════════════════════════════════╗"
        echo "  ║   Installation complete.                     ║"
        echo "  ║                                             ║"
        echo "  ║   You can now build the project with:       ║"
        echo "  ║     make                                    ║"
        echo "  ║                                             ║"
        echo "  ║   Then run with:                            ║"
        echo "  ║     ./swordigo_boot                         ║"
        echo "  ╚══════════════════════════════════════════════╝"
        echo ""
        exit 0
    else
        echo "  ╔══════════════════════════════════════════════╗"
        echo "  ║   Installation completed with ${errors} error(s).     ║"
        echo "  ║   Check the messages above.                 ║"
        echo "  ╚══════════════════════════════════════════════╝"
        echo ""
        exit 1
    fi
}

main "$@"

