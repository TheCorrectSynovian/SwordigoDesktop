# Packaging Consistency Audit

## Issues Found

| # | Issue | Affects | Severity |
|---|-------|---------|----------|
| 1 | DEB missing `libvorbis0a` dependency | DEB | 🔴 Music won't play |
| 2 | AppImage not bundling `libvorbisfile` | AppImage | 🔴 Music won't play |
| 3 | AppImage not bundling `libSDL2_image` | AppImage | 🔴 Icon won't load |
| 4 | Icon path missing AppImage `SWORDIGO_DATA_DIR` fallback in display.cpp | AppImage | 🟡 No window icon |
| 5 | RPM changelog says "WAV music" but we switched to OGG | RPM | 🟢 Cosmetic |

## Fixes Applied

### 1. DEB control — add libvorbis
### 2. AppImage — bundle libvorbisfile + libSDL2_image
### 3. display.cpp — use get_data_path() for icon
### 4. RPM changelog — update to mention OGG
