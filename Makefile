## Swordigo Desktop - Makefile
## Usage: make -j$(nproc) && ./swordigo_boot

CXX     := g++
CC      := gcc
CXXFLAGS := -std=c++17 -g -O1 -Isrc -Isrc/imgui -Iinclude -MMD -MP
CFLAGS   := -g -O1 -Isrc -Iinclude -MMD -MP

# pkg-config queries
SDL3_CFLAGS  := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LIBS    := $(shell pkg-config --libs sdl3 2>/dev/null)
SDL3I_CFLAGS := $(shell pkg-config --cflags SDL3_image 2>/dev/null || pkg-config --cflags sdl3-image 2>/dev/null)
SDL3I_LIBS   := $(shell pkg-config --libs SDL3_image 2>/dev/null || pkg-config --libs sdl3-image 2>/dev/null || echo "-lSDL3_image")
VORB_CFLAGS  := $(shell pkg-config --cflags vorbisfile 2>/dev/null)
VORB_LIBS    := $(shell pkg-config --libs vorbisfile 2>/dev/null)
ZLIB_LIBS    := $(shell pkg-config --libs zlib 2>/dev/null || echo "-lz")
OAL_LIBS     := $(shell pkg-config --libs openal 2>/dev/null || echo "-lopenal")

ALL_CXXFLAGS := $(CXXFLAGS) $(SDL3_CFLAGS) $(SDL3I_CFLAGS) $(VORB_CFLAGS)
ALL_CFLAGS   := $(CFLAGS) $(SDL3_CFLAGS)

LIBS := $(SDL3_LIBS) $(SDL3I_LIBS) $(VORB_LIBS) $(ZLIB_LIBS) $(OAL_LIBS) \
        -lunicorn -lGL -lpthread -lm

# Source files
CXX_SRCS := \
    src/main.cpp \
    src/jni/jni_marshaller.cpp \
    src/platform/display.cpp \
    src/loader/elf_loader.cpp \
    src/loader/elf_loader_arm64.cpp \
    src/jni/jni_bridge.cpp \
    src/jni/jni_bridge_arm64.cpp \
    src/platform/emulator.cpp \
    src/platform/emulator_arm64.cpp \
    src/platform/gui.cpp \
    src/platform/input_config.cpp \
    src/game/camera_override.cpp \
    src/game/mod_tools.cpp \
    src/game/save_editor_logic.cpp \
    src/platform/fbo_scaler.cpp \
    src/platform/pvr_loader.cpp \
    src/platform/io_thread.cpp \
    src/platform/data_path.cpp \
    src/platform/binary_selector.cpp \
    src/platform/launcher_ui.cpp \
    src/platform/save_editor.cpp \
    src/platform/srt_overlay.cpp \
    src/imgui/imgui.cpp \
    src/imgui/imgui_draw.cpp \
    src/imgui/imgui_tables.cpp \
    src/imgui/imgui_widgets.cpp \
    src/imgui/backends/imgui_impl_sdl3.cpp \
    src/imgui/backends/imgui_impl_opengl3.cpp

C_SRCS := \
    src/android/asset_manager.c \
    src/android/log.c

# Object files (in build/ dir)
CXX_OBJS := $(patsubst src/%.cpp, build/%.o, $(CXX_SRCS))
C_OBJS   := $(patsubst src/%.c, build/%.o, $(C_SRCS))
ALL_OBJS := $(CXX_OBJS) $(C_OBJS)

# Auto-generated dependency files
DEPS := $(ALL_OBJS:.o=.d)

# Default target
all: swordigo_boot libsre.so

swordigo_boot: $(ALL_OBJS)
	@echo "[LINK] $@"
	@$(CXX) -o $@ $^ $(LIBS)

# =========================================================================
# libsre.so — Swordigo Runtime Engine (ARM64 cross-compiled)
# =========================================================================
# This is a guest-side library loaded into the Unicorn emulator.
# It replaces problematic functions in libswordigo.so with clean C code.
AARCH64_CC := aarch64-linux-gnu-gcc
SRE_SRCS   := src/sre/sre_init.c src/sre/sre_string.c src/sre/sre_lua.c src/sre/sre_background.c src/sre/sre_effects.c src/sre/sre_music.c src/sre/sre_gui.c src/sre/sre_gui_native.c src/sre/sre_scene_update.c src/sre/sre_setjmp.S src/sre/sre_mini_api.c src/sre/sre_vfs.c src/sre/sre_lua_libs.c src/sre/sre_mod.c
SRE_CFLAGS := -shared -fPIC -O2 -nostdlib -fno-builtin -Isrc/sre

libsre.so: $(SRE_SRCS) src/sre/sre.h src/sre/sre_lua.h src/sre/sre_setjmp.h src/sre/sre_gui.h
	@echo "[SRE]  Building libsre.so (ARM64)"
	@$(AARCH64_CC) $(SRE_CFLAGS) -o $@ $(SRE_SRCS)
	@echo "[SRE]  Built libsre.so (ARM64) — ready for emulator loading"

# Install libsre.so alongside libswordigo.so
install-sre: libsre.so
	@mkdir -p $(HOME)/.local/share/swordigo-desktop/engine/v1.4.12/arm64-v8a/
	@cp libsre.so $(HOME)/.local/share/swordigo-desktop/engine/v1.4.12/arm64-v8a/libsre.so
	@echo "[SRE]  Installed to ~/.local/share/swordigo-desktop/engine/v1.4.12/arm64-v8a/"

# =========================================================================
# asset_viewer — Standalone asset browser/previewer tool
# =========================================================================
# Separate binary with minimal dependencies (no unicorn, no game code).
AV_SRCS := src/tools/asset_viewer.cpp \
    src/tools/pod_loader.cpp src/tools/av_renderer.cpp \
    src/tools/av_audio.cpp src/tools/scene_loader.cpp \
    src/platform/pvr_loader.cpp \
    src/imgui/imgui.cpp src/imgui/imgui_draw.cpp \
    src/imgui/imgui_tables.cpp src/imgui/imgui_widgets.cpp \
    src/imgui/backends/imgui_impl_sdl3.cpp src/imgui/backends/imgui_impl_opengl3.cpp
AV_CXXFLAGS := -std=c++17 -g -O2 -Isrc -Isrc/imgui -Iinclude $(SDL3_CFLAGS) $(SDL3I_CFLAGS)
AV_LIBS := $(SDL3_LIBS) $(SDL3I_LIBS) -lGL

asset_viewer: $(AV_SRCS)
	@echo "[AV]   Building asset_viewer"
	@$(CXX) $(AV_CXXFLAGS) -o $@ $(AV_SRCS) $(AV_LIBS)
	@echo "[AV]   Built asset_viewer — run with: ./asset_viewer"

# C++ compile rule
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX]  $<"
	@$(CXX) $(ALL_CXXFLAGS) -c $< -o $@

# C compile rule
build/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]   $<"
	@$(CC) $(ALL_CFLAGS) -c $< -o $@

# Include auto-generated dependency files (if they exist)
-include $(DEPS)

clean:
	rm -f $(ALL_OBJS) $(DEPS) swordigo_boot libsre.so asset_viewer

.PHONY: all clean install-sre asset_viewer

