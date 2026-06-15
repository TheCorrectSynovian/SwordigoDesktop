CC=g++
CFLAGS=-Isrc -std=c++17
LIBS=-lunicorn
SDL_FLAGS=$(shell pkg-config --cflags --libs sdl2)
GL_FLAGS=-lGL

COMMON_SRC=src/loader/elf_loader.cpp src/jni/jni_bridge.cpp src/platform/emulator.cpp src/android/asset_manager.c src/android/log.c

all: swordigo_boot swordigo_headless loader_test

# Visual mode (SDL2 + OpenGL)
swordigo_boot: src/main.cpp src/jni/jni_marshaller.cpp src/platform/display.cpp $(COMMON_SRC)
	$(CC) $(CFLAGS) $(SDL_FLAGS) $^ -o $@ $(LIBS) $(GL_FLAGS)

# Headless mode (no display dependencies)
swordigo_headless: src/main.cpp src/jni/jni_marshaller.cpp src/platform/display.cpp $(COMMON_SRC)
	$(CC) $(CFLAGS) $(SDL_FLAGS) $^ -o $@ $(LIBS) $(GL_FLAGS) -DHEADLESS_DEFAULT

loader_test: src/loader_test.cpp $(COMMON_SRC)
	$(CC) $(CFLAGS) $(SDL_FLAGS) $^ -o $@ $(LIBS) $(GL_FLAGS)

clean:
	rm -f loader_test swordigo_boot swordigo_headless
