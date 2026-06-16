CC=g++
CFLAGS=-Isrc -std=c++17
LIBS=-lunicorn -pthread
SDL_FLAGS=$(shell pkg-config --cflags --libs sdl2)
GL_FLAGS=-lGL -lz -lopenal -lSDL2_image

COMMON_SRC=src/loader/elf_loader.cpp src/jni/jni_bridge.cpp src/platform/emulator.cpp src/android/asset_manager.c src/android/log.c src/platform/gui.cpp src/platform/input_config.cpp src/game/camera_override.cpp src/game/mod_tools.cpp src/platform/fbo_scaler.cpp src/platform/io_thread.cpp

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

# ---- Installation ----
PREFIX ?= /usr/local
DATADIR = $(PREFIX)/share/swordigo
ICON_SRC = src/assets/icon_gnome.png
DESKTOP_FILE = packaging/swordigo-desktop.desktop

install: swordigo_boot
	install -Dm755 swordigo_boot $(DESTDIR)$(PREFIX)/bin/swordigo-desktop
	# Game library
	install -Dm644 libswordigo.so $(DESTDIR)$(DATADIR)/libswordigo.so
	# Game assets (textures, models, sounds, music, lua)
	mkdir -p $(DESTDIR)$(DATADIR)/assets/resources
	cp -r assets/resources/* $(DESTDIR)$(DATADIR)/assets/resources/
	# Vita port assets (icons, startup screen)
	mkdir -p $(DESTDIR)$(DATADIR)/src/assets
	cp src/assets/*.png $(DESTDIR)$(DATADIR)/src/assets/ 2>/dev/null || true
	# Icons + desktop entry
	install -Dm644 $(ICON_SRC) $(DESTDIR)$(PREFIX)/share/icons/hicolor/128x128/apps/swordigo-desktop.png
	install -Dm644 $(ICON_SRC) $(DESTDIR)$(PREFIX)/share/pixmaps/swordigo-desktop.png
	install -Dm644 $(DESKTOP_FILE) $(DESTDIR)$(PREFIX)/share/applications/swordigo-desktop.desktop
	@echo "[Install] Installed to $(DESTDIR)$(PREFIX)"
	@echo "[Install] Data dir: $(DESTDIR)$(DATADIR)"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/swordigo-desktop
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/128x128/apps/swordigo-desktop.png
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/swordigo-desktop.png
	rm -f $(DESTDIR)$(PREFIX)/share/applications/swordigo-desktop.desktop
	@echo "[Uninstall] Removed from $(DESTDIR)$(PREFIX)"
