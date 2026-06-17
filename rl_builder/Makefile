# RLSwordigo Builder — Manages RL mod assets and packaging
# This Makefile copies RL assets and .so files for packaging alongside vanilla Swordigo

BASEDIR := $(shell cd "$(dir $(lastword $(MAKEFILE_LIST)))/.." && pwd)
DESTDIR ?= $(BASEDIR)/packaging

RL_ASSETS := $(BASEDIR)/rl_assets
RL_LIBS := $(BASEDIR)/rl_libswordigo.so $(BASEDIR)/rl_libmini.so $(BASEDIR)/rl_libGlossHook.so

# Install RL assets + libs to DEB packaging tree
install-deb:
	@echo "[RL] Installing RLSwordigo to DEB tree..."
	mkdir -p $(DESTDIR)/deb/usr/share/swordigo/rl_assets
	cp -r $(RL_ASSETS)/* $(DESTDIR)/deb/usr/share/swordigo/rl_assets/
	cp $(BASEDIR)/rl_libswordigo.so $(DESTDIR)/deb/usr/share/swordigo/
	cp $(BASEDIR)/rl_libmini.so $(DESTDIR)/deb/usr/share/swordigo/
	cp $(BASEDIR)/rl_libGlossHook.so $(DESTDIR)/deb/usr/share/swordigo/
	@echo "[RL] DEB install complete"

# Install RL assets to a build output directory
install-build:
	@echo "[RL] Copying RL files to build/v2..."
	mkdir -p $(BASEDIR)/build/v2/rl_assets
	cp -r $(RL_ASSETS)/* $(BASEDIR)/build/v2/rl_assets/
	cp $(BASEDIR)/rl_libswordigo.so $(BASEDIR)/build/v2/
	cp $(BASEDIR)/rl_libmini.so $(BASEDIR)/build/v2/
	cp $(BASEDIR)/rl_libGlossHook.so $(BASEDIR)/build/v2/
	@echo "[RL] Build copy complete"

# Check that all RL files exist
check:
	@echo "[RL] Checking RLSwordigo files..."
	@test -d "$(RL_ASSETS)" || (echo "ERROR: rl_assets/ not found" && exit 1)
	@test -f "$(BASEDIR)/rl_libswordigo.so" || (echo "ERROR: rl_libswordigo.so not found" && exit 1)
	@test -f "$(BASEDIR)/rl_libmini.so" || (echo "ERROR: rl_libmini.so not found" && exit 1)
	@test -f "$(BASEDIR)/rl_libGlossHook.so" || (echo "ERROR: rl_libGlossHook.so not found" && exit 1)
	@echo "[RL] All files present"
	@echo "  rl_libswordigo.so: $$(sha256sum "$(BASEDIR)/rl_libswordigo.so" | cut -c1-16)..."
	@echo "  rl_libmini.so:     $$(sha256sum "$(BASEDIR)/rl_libmini.so" | cut -c1-16)..."
	@echo "  rl_libGlossHook.so: $$(sha256sum "$(BASEDIR)/rl_libGlossHook.so" | cut -c1-16)..."
	@echo "  rl_assets: $$(ls "$(RL_ASSETS)/resources/" | wc -l) resource files"

.PHONY: install-deb install-build check
