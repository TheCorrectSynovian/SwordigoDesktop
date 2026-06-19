cmake_minimum_required(VERSION 3.12)
project(SwordigoDesktop VERSION 4.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenAL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
find_package(SDL3_image REQUIRED)

# Unicorn emulator library manual search
find_library(UNICORN_LIBRARY NAMES unicorn REQUIRED)
find_path(UNICORN_INCLUDE_DIR NAMES unicorn/unicorn.h REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../include
    ${OPENGL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${UNICORN_INCLUDE_DIR}
)

# Source files (must match the root CMakeLists.txt)
set(SOURCES
    ${CMAKE_SOURCE_DIR}/../src/main.cpp
    ${CMAKE_SOURCE_DIR}/../src/jni/jni_marshaller.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/display.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/gui.cpp
    ${CMAKE_SOURCE_DIR}/../src/loader/elf_loader.cpp
    ${CMAKE_SOURCE_DIR}/../src/loader/elf_loader_arm64.cpp
    ${CMAKE_SOURCE_DIR}/../src/jni/jni_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../src/jni/jni_bridge_arm64.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/emulator.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/emulator_arm64.cpp
    ${CMAKE_SOURCE_DIR}/../src/android/asset_manager.c
    ${CMAKE_SOURCE_DIR}/../src/android/log.c
    ${CMAKE_SOURCE_DIR}/../src/platform/input_config.cpp
    ${CMAKE_SOURCE_DIR}/../src/game/camera_override.cpp
    ${CMAKE_SOURCE_DIR}/../src/game/mod_tools.cpp
    ${CMAKE_SOURCE_DIR}/../src/game/save_editor_logic.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/fbo_scaler.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/io_thread.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/data_path.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/binary_selector.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/launcher.cpp
    ${CMAKE_SOURCE_DIR}/../src/platform/vulkan_backend.cpp
)

# Add SWORDIGO_DATA_DIR definition
set(SWORDIGO_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/swordigo/")
add_compile_definitions(SWORDIGO_DATA_DIR_PATH="${SWORDIGO_DATA_DIR}")

# Executable
add_executable(swordigo_boot ${SOURCES})
target_link_libraries(swordigo_boot
    SDL3::SDL3
    OpenGL::GL
    ZLIB::ZLIB
    OpenAL::OpenAL
    ${UNICORN_LIBRARY}
    ${VORBISFILE_LIBRARIES}
    SDL3_image::SDL3_image
    pthread
)

# ============================================================================
# Installation Rules
# ============================================================================

install(TARGETS swordigo_boot DESTINATION bin)

# Game assets directory (user must supply assets/ from their own APK)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../assets DESTINATION share/swordigo
        PATTERN "resources/*" EXCLUDE
        PATTERN "dexopt/*" EXCLUDE)

# Ship BOTH game binaries so users can choose in the launcher
install(FILES ${CMAKE_SOURCE_DIR}/../libswordigo_nx.so DESTINATION share/swordigo
        OPTIONAL)
install(FILES ${CMAKE_SOURCE_DIR}/../libswordigo.so DESTINATION share/swordigo
        OPTIONAL)

# Launcher assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/../src/assets/ DESTINATION share/swordigo/src/assets)

# Desktop entry
install(FILES ${CMAKE_SOURCE_DIR}/../Swordigo.desktop DESTINATION share/applications)

# ============================================================================
# Packaging via CPack
# ============================================================================

set(CPACK_PACKAGE_NAME "swordigo-desktop")
set(CPACK_PACKAGE_VENDOR "Prenxy / TheCorrectSynovian")
set(CPACK_PACKAGE_VERSION "4.5.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Swordigo Desktop v4.5r — Native Linux port with dual-arch ARM emulation")
set(CPACK_PACKAGE_CONTACT "quantumcreeper@gmail.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")

# Generators to build
set(CPACK_GENERATOR "RPM;DEB")

# --- RPM packaging ---
set(CPACK_RPM_PACKAGE_REQUIRES "SDL3, SDL3_image, openal-soft, mesa-libGL, zlib, unicorn, libvorbis")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
# Disable auto-dependency scanning — the shipped .so files are Android ARM
# binaries that rpmbuild incorrectly scans for x86_64 Linux deps
set(CPACK_RPM_PACKAGE_AUTOREQ "no")
set(CPACK_RPM_PACKAGE_AUTOPROV "no")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Swordigo Desktop v4.5r — A native Linux runtime for Swordigo (ARM emulation via Unicorn).
Features: SDL3, advanced shaders (SSAO, godrays, tone mapping), gamepad support,
save editor, ARM32+ARM64, binary selector, and more.
NOTE: Game assets (assets/) and binary (.so) must be supplied by the user from their own copy.")

# --- Debian packaging ---
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-0, libsdl3-image-0, libopenal1, libgl1-mesa-glx | libgl1, zlib1g, libunicorn2 | libunicorn1, libvorbisfile3")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

include(CPack)
