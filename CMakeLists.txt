cmake_minimum_required(VERSION 3.16)
project(SwordigoDesktop CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source lists
set(COMMON_SOURCES
    src/loader/elf_loader.cpp
    src/jni/jni_bridge.cpp
    src/platform/emulator.cpp
    src/android/asset_manager.c
    src/android/log.c
    src/platform/gui.cpp
    src/platform/input_config.cpp
    src/game/camera_override.cpp
    src/game/mod_tools.cpp
    src/platform/fbo_scaler.cpp
    src/platform/io_thread.cpp
    src/platform/data_path.cpp
    src/platform/binary_selector.cpp
    src/platform/launcher.cpp
)

set(MAIN_SOURCES
    src/main.cpp
    src/jni/jni_marshaller.cpp
    src/platform/display.cpp
)

# Header search paths
include_directories(src include)

# Find Package Dependencies
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenAL REQUIRED)

# Find Unicorn and Vorbisfile
if(WIN32)
    # On Windows, look for libraries via standard names or vcpkg
    find_library(UNICORN_LIB NAMES unicorn REQUIRED)
    find_library(VORBISFILE_LIB NAMES vorbisfile vorbisfile_static REQUIRED)
    find_library(SDL2_IMAGE_LIB NAMES SDL2_image REQUIRED)
else()
    # On Linux, try PkgConfig first for vorbisfile and sdl2_image
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    find_library(UNICORN_LIB NAMES unicorn REQUIRED)
endif()

# Graphics API selection: OpenGL by default, optional Vulkan target
option(ENABLE_VULKAN "Build with Vulkan support" OFF)

if(ENABLE_VULKAN)
    add_definitions(-DVULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
    set(VULKAN_SRC src/platform/vulkan_backend.cpp)
endif()

# ============================================================================
# Target 1: swordigo_boot (Default Visual Mode)
# ============================================================================
add_executable(swordigo_boot ${MAIN_SOURCES} ${COMMON_SOURCES} ${VULKAN_SRC})

if(WIN32)
    target_link_libraries(swordigo_boot 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIB}
        ${SDL2_IMAGE_LIB}
    )
else()
    target_link_libraries(swordigo_boot 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        pthread
    )
endif()

# ============================================================================
# Target 2: swordigo_headless
# ============================================================================
add_executable(swordigo_headless ${MAIN_SOURCES} ${COMMON_SOURCES})
target_compile_definitions(swordigo_headless PRIVATE HEADLESS_DEFAULT)

if(WIN32)
    target_link_libraries(swordigo_headless 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIB}
        ${SDL2_IMAGE_LIB}
    )
else()
    target_link_libraries(swordigo_headless 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        pthread
    )
endif()

# ============================================================================
# Target 3: loader_test
# ============================================================================
add_executable(loader_test src/loader_test.cpp ${COMMON_SOURCES})

if(WIN32)
    target_link_libraries(loader_test 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIB}
        ${SDL2_IMAGE_LIB}
    )
else()
    target_link_libraries(loader_test 
        SDL2::SDL2
        OpenGL::GL
        ZLIB::ZLIB
        OpenAL::OpenAL
        ${UNICORN_LIB}
        ${VORBISFILE_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        pthread
    )
endif()
