#!/bin/bash
# Swordigo Desktop Launcher

# Ensure we are in the correct directory
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Detection of session type
if [ "$XDG_SESSION_TYPE" == "wayland" ]; then
    echo "Running on Wayland session..."
    # Potential Wayland-specific tweaks (e.g., SDL_VIDEODRIVER=wayland)
    # export SDL_VIDEODRIVER=wayland
fi

# Run the boot prototype
./swordigo_boot "$@"
