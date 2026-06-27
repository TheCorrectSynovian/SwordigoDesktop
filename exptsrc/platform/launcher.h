// Unified pre-launch configuration window for Swordigo Desktop
// Borderless SDL3 + OpenGL window combining graphics API + binary selection

#pragma once

#include "platform/vulkan_backend.h"  // for GraphicsAPI enum
#include "platform/binary_selector.h" // for BinarySelector, BinaryInfo
#include <string>

struct LaunchConfig {
    GraphicsAPI graphics_api = GraphicsAPI::OPENGL;
    std::string selected_binary = "engine/v1.4.12/armeabi-v7a/libswordigo.so";
    std::string assets_dir = "assets";       // "assets" for vanilla, "rl_assets" for RLSwordigo
    std::string game_type = "Swordigo";      // "Swordigo" or "RLSwordigo"
    bool should_launch = true;  // false if user closed the launcher
};

// Show the unified launcher window and block until user clicks Launch or closes.
// Creates its own borderless SDL_Window + GL context, cleans up before returning.
// If selector has binaries, they are shown as a list; otherwise that section is hidden.
LaunchConfig show_launcher(BinarySelector& selector);
