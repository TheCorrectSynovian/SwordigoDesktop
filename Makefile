## Swordigo Desktop - Makefile
## Usage: make -j$(nproc) && ./swordigo_boot

CXX     := g++
CC      := gcc
CXXFLAGS := -std=c++17 -g -O1 -Isrc -Iinclude -MMD -MP
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
    src/platform/io_thread.cpp \
    src/platform/data_path.cpp \
    src/platform/binary_selector.cpp \
    src/platform/launcher.cpp \
    src/platform/save_editor.cpp

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
all: swordigo_boot

swordigo_boot: $(ALL_OBJS)
	@echo "[LINK] $@"
	@$(CXX) -o $@ $^ $(LIBS)

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
	rm -f $(ALL_OBJS) $(DEPS) swordigo_boot

.PHONY: all clean
