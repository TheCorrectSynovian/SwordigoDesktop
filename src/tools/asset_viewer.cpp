/* asset_viewer.cpp — Standalone asset viewer for Swordigo Desktop
 *
 * A self-contained tool that lets you browse and preview game assets:
 *   - PVR textures (ETC1, decoded via pvr_loader)
 *   - PNG textures (via SDL3_image)
 *   - Audio files (WAV/OGG, metadata display)
 *   - Scene/model files (.pod, .scene, metadata display)
 *
 * Build:
 *   make asset_viewer
 *
 * Usage:
 *   ./asset_viewer [optional_start_directory]
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <GL/gl.h>

// pvr_loader.h uses gl_inc.h which needs GL/gl.h — already included above
#include "platform/pvr_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <fstream>

namespace fs = std::filesystem;

// ============================================================================
// Configuration
// ============================================================================

static const int WIN_W = 1200;
static const int WIN_H = 800;
static const char* WIN_TITLE = "Swordigo Asset Viewer";

// Layout constants
static const float LEFT_PANEL_W  = 340.0f;
static const float TOP_BAR_H     = 36.0f;
static const float FILTER_BAR_H  = 32.0f;
static const float INFO_BAR_H    = 80.0f;
static const float FILE_ROW_H    = 22.0f;
static const float SCROLLBAR_W   = 12.0f;
static const float TEXT_SCALE     = 1.5f;
static const float SMALL_SCALE   = 1.2f;

// Colors (dark premium theme)
static const uint8_t BG_R = 20, BG_G = 20, BG_B = 28;
static const uint8_t PANEL_R = 28, PANEL_G = 28, PANEL_B = 38;
static const uint8_t BAR_R = 32, BAR_G = 32, BAR_B = 44;
static const uint8_t HOVER_R = 50, HOVER_G = 50, HOVER_B = 70;
static const uint8_t SEL_R = 70, SEL_G = 100, SEL_B = 180;
static const uint8_t TEXT_R = 210, TEXT_G = 210, TEXT_B = 220;
static const uint8_t DIM_R = 120, DIM_G = 120, DIM_B = 140;
static const uint8_t ACCENT_R = 100, ACCENT_G = 160, ACCENT_B = 255;
static const uint8_t DIR_R = 255, DIR_G = 200, DIR_B = 80;
static const uint8_t BORDER_R = 55, BORDER_G = 55, BORDER_B = 70;

// Default asset directories
static const char* DEFAULT_DIRS[] = {
    "~/.local/share/swordigo-desktop/assets/resources/",
    "~/.local/share/swordigo-desktop/rl_assets/resources/",
};

// ============================================================================
// 8x8 Bitmap Font (copied from gui.cpp — ASCII 32..127)
// ============================================================================

static const uint8_t font_8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00}, // #
    {0x0c, 0x3f, 0x0c, 0x0e, 0x3c, 0x0c, 0x3e, 0x0c}, // $
    {0x00, 0x66, 0x66, 0x30, 0x18, 0x0c, 0x66, 0x66}, // %
    {0x3c, 0x66, 0x3c, 0x38, 0x67, 0x66, 0x3f, 0x00}, // &
    {0x06, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c}, // (
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30}, // )
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // ,
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x00, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00}, // /
    {0x3e, 0x63, 0x67, 0x6f, 0x7b, 0x63, 0x3e, 0x00}, // 0
    {0x0c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3e, 0x00}, // 1
    {0x3e, 0x63, 0x06, 0x1c, 0x30, 0x60, 0x7f, 0x00}, // 2
    {0x7f, 0x06, 0x0c, 0x1c, 0x06, 0x63, 0x3e, 0x00}, // 3
    {0x06, 0x0f, 0x1b, 0x33, 0x7f, 0x03, 0x03, 0x00}, // 4
    {0x7f, 0x60, 0x7e, 0x03, 0x03, 0x63, 0x3e, 0x00}, // 5
    {0x1c, 0x30, 0x60, 0x7e, 0x63, 0x63, 0x3e, 0x00}, // 6
    {0x7f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x00}, // 7
    {0x3e, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x3e, 0x00}, // 8
    {0x3e, 0x63, 0x63, 0x7f, 0x03, 0x06, 0x3c, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ;
    {0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00}, // <
    {0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00}, // =
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00}, // >
    {0x3e, 0x63, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00}, // ?
    {0x3e, 0x63, 0x6f, 0x6b, 0x6b, 0x60, 0x3e, 0x00}, // @
    {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00}, // A
    {0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00}, // B
    {0x3e, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3e, 0x00}, // C
    {0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00}, // D
    {0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x7e, 0x00}, // E
    {0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3e, 0x63, 0x60, 0x6e, 0x63, 0x63, 0x3e, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00}, // H
    {0x3e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3e, 0x00}, // I
    {0x1f, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3c, 0x00}, // J
    {0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7f, 0x00}, // L
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00}, // M
    {0x63, 0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x00}, // N
    {0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00}, // O
    {0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3e, 0x63, 0x63, 0x63, 0x6b, 0x66, 0x3d, 0x00}, // Q
    {0x7c, 0x66, 0x66, 0x7c, 0x78, 0x6c, 0x66, 0x00}, // R
    {0x3e, 0x63, 0x38, 0x0e, 0x07, 0x63, 0x3e, 0x00}, // S
    {0x7f, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x00}, // T
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00}, // U
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00}, // W
    {0x63, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x63, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7f, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7f, 0x00}, // Z
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00}, // [
    {0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00}, // backslash
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00}, // ]
    {0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff}, // _
    {0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
    {0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3b, 0x00}, // a
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x7c, 0x00}, // b
    {0x00, 0x00, 0x3c, 0x60, 0x60, 0x62, 0x3c, 0x00}, // c
    {0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00}, // d
    {0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00}, // e
    {0x0e, 0x18, 0x3e, 0x18, 0x18, 0x18, 0x18, 0x00}, // f
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // g
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // h
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00}, // i
    {0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x3c}, // j
    {0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00}, // k
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00}, // l
    {0x00, 0x00, 0x66, 0x7f, 0x6b, 0x6b, 0x63, 0x00}, // m
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // n
    {0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00}, // o
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60}, // p
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06}, // q
    {0x00, 0x00, 0x7c, 0x66, 0x60, 0x60, 0x60, 0x00}, // r
    {0x00, 0x00, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x00}, // s
    {0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x0d, 0x06}, // t
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3b, 0x00}, // u
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // v
    {0x00, 0x00, 0x63, 0x6b, 0x6b, 0x7f, 0x36, 0x00}, // w
    {0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00}, // x
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // y
    {0x00, 0x00, 0x7e, 0x0c, 0x18, 0x30, 0x7e, 0x00}, // z
    {0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00}, // {
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // |
    {0x30, 0x18, 0x18, 0x0c, 0x18, 0x18, 0x30, 0x00}, // }
    {0x3b, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
    {0x1c, 0x36, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00}  // bullet
};

// ============================================================================
// Drawing primitives (adapted from gui.cpp — standalone, no class)
// ============================================================================

// NOTE: The coordinate system is top-left origin (Y increases downward),
// set up via glOrtho(0, W, H, 0, -1, 1). This differs from gui.cpp which
// uses bottom-left origin, so draw_char renders top-down.

static void draw_rect(float x, float y, float w, float h,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void draw_border(float x, float y, float w, float h,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glColor4ub(r, g, b, a);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void draw_char(char c, float x, float y, float scale,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    int idx = (uint8_t)c - 32;
    if (idx < 0 || idx >= 96) return;
    const uint8_t* glyph = font_8x8[idx];

    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    for (int row = 0; row < 8; ++row) {
        uint8_t row_data = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (row_data & (1 << (7 - col))) {
                // Top-left origin: row 0 is top of glyph
                float px = x + col * scale;
                float py = y + row * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
    }
    glEnd();
}

static void draw_string(const char* str, float x, float y, float scale,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    float cur_x = x;
    while (*str) {
        if (*str == '\n') {
            y += 10.0f * scale;
            cur_x = x;
            str++;
            continue;
        }
        draw_char(*str, cur_x, y, scale, r, g, b, a);
        cur_x += 8.0f * scale;
        str++;
    }
}

static void draw_string(const std::string& str, float x, float y, float scale,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    draw_string(str.c_str(), x, y, scale, r, g, b, a);
}

static float text_width(const char* str, float scale) {
    float max_w = 0, cur_w = 0;
    while (*str) {
        if (*str == '\n') { max_w = std::max(max_w, cur_w); cur_w = 0; }
        else cur_w += 8.0f * scale;
        str++;
    }
    return std::max(max_w, cur_w);
}

static float text_width(const std::string& s, float scale) {
    return text_width(s.c_str(), scale);
}

// Draw a textured quad (for texture preview)
static void draw_textured_quad(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// ============================================================================
// File type helpers
// ============================================================================

enum FileType {
    FT_UNKNOWN,
    FT_DIRECTORY,
    FT_TEXTURE_PVR,
    FT_TEXTURE_PNG,
    FT_TEXTURE_JPG,
    FT_MODEL_POD,
    FT_SCENE,
    FT_AUDIO_WAV,
    FT_AUDIO_OGG,
    FT_LUA,
    FT_XML,
    FT_OTHER,
};

enum FilterMode {
    FILTER_ALL,
    FILTER_TEXTURES,
    FILTER_MODELS,
    FILTER_SCENES,
    FILTER_AUDIO,
    FILTER_COUNT,
};

static const char* filter_names[FILTER_COUNT] = {
    "All", "Textures", "Models", "Scenes", "Audio"
};

static FileType classify_file(const std::string& name, bool is_dir) {
    if (is_dir) return FT_DIRECTORY;

    // Get extension (lowercased)
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return FT_OTHER;
    std::string ext = name.substr(dot);
    for (auto& c : ext) c = tolower(c);

    if (ext == ".pvr")   return FT_TEXTURE_PVR;
    if (ext == ".png")   return FT_TEXTURE_PNG;
    if (ext == ".jpg" || ext == ".jpeg") return FT_TEXTURE_JPG;
    if (ext == ".pod")   return FT_MODEL_POD;
    if (ext == ".scene") return FT_SCENE;
    if (ext == ".wav")   return FT_AUDIO_WAV;
    if (ext == ".ogg")   return FT_AUDIO_OGG;
    if (ext == ".lua")   return FT_LUA;
    if (ext == ".xml")   return FT_XML;
    return FT_OTHER;
}

static bool passes_filter(FileType ft, FilterMode filter) {
    if (filter == FILTER_ALL) return true;
    if (ft == FT_DIRECTORY) return true; // always show directories
    switch (filter) {
        case FILTER_TEXTURES: return (ft == FT_TEXTURE_PVR || ft == FT_TEXTURE_PNG || ft == FT_TEXTURE_JPG);
        case FILTER_MODELS:   return (ft == FT_MODEL_POD);
        case FILTER_SCENES:   return (ft == FT_SCENE);
        case FILTER_AUDIO:    return (ft == FT_AUDIO_WAV || ft == FT_AUDIO_OGG);
        default: return true;
    }
}

static const char* filetype_label(FileType ft) {
    switch (ft) {
        case FT_DIRECTORY:   return "DIR";
        case FT_TEXTURE_PVR: return "PVR";
        case FT_TEXTURE_PNG: return "PNG";
        case FT_TEXTURE_JPG: return "JPG";
        case FT_MODEL_POD:   return "POD";
        case FT_SCENE:       return "SCENE";
        case FT_AUDIO_WAV:   return "WAV";
        case FT_AUDIO_OGG:   return "OGG";
        case FT_LUA:         return "LUA";
        case FT_XML:         return "XML";
        default:             return "FILE";
    }
}

static void filetype_color(FileType ft, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (ft) {
        case FT_DIRECTORY:   r = DIR_R; g = DIR_G; b = DIR_B; break;
        case FT_TEXTURE_PVR: r = 100; g = 200; b = 100; break;
        case FT_TEXTURE_PNG: r = 100; g = 220; b = 150; break;
        case FT_TEXTURE_JPG: r = 100; g = 200; b = 180; break;
        case FT_MODEL_POD:   r = 200; g = 130; b = 255; break;
        case FT_SCENE:       r = 255; g = 150; b = 100; break;
        case FT_AUDIO_WAV:
        case FT_AUDIO_OGG:   r = 100; g = 180; b = 255; break;
        case FT_LUA:         r = 180; g = 180; b = 255; break;
        case FT_XML:         r = 180; g = 200; b = 180; break;
        default:             r = DIM_R; g = DIM_G; b = DIM_B; break;
    }
}

// ============================================================================
// File entry
// ============================================================================

struct FileEntry {
    std::string name;
    std::string full_path;
    FileType    type;
    uintmax_t   size;
    bool        is_dir;
};

// ============================================================================
// Application State
// ============================================================================

// ============================================================================
// POD 3D Model Structures
// ============================================================================

struct PODMesh {
    std::vector<float> positions;   // x,y,z triples
    std::vector<float> normals;     // x,y,z triples
    std::vector<float> uvs;         // u,v pairs (texture coordinates)
    std::vector<uint16_t> indices;  // triangle indices (uint16)
    int num_vertices = 0;
    int num_faces = 0;
    // Bounding box
    float min_x = 1e9f, min_y = 1e9f, min_z = 1e9f;
    float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
};

struct PODModel {
    std::vector<PODMesh> meshes;
    std::string version;
    int num_frames = 0;
    // Overall bounding box
    float min_x = 1e9f, min_y = 1e9f, min_z = 1e9f;
    float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
    float center_x = 0, center_y = 0, center_z = 0;
    float radius = 1.0f;
    int total_vertices = 0;
    int total_faces = 0;
};

// Minimal POD Parser (PowerVR PVRTools SDK chunk-based format)
// POD files use tag-length-data: uint32 tag, uint32 length, [data]
// Container blocks: length=0, closed by (tag | 0x80000000)
//
// PVRTools SDK tag IDs:
//   File:  1000=Version, 1001=Scene(ctr)
//   Scene: 2004=NumMesh, 2009=NumFrame, 2012=Mesh(ctr)
//   Mesh:  3000=NumVtx, 3001=NumFaces, 3003=Faces(ctr),
//          3006=InterleavedData, 3050=Vertices(ctr), 3051=Normals(ctr)
//   Data:  4000=Type, 4001=N, 4002=Stride, 4003=Data

static bool pod_read_u32(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
    if (p + 4 > end) return false;
    memcpy(&out, p, 4); p += 4;
    return true;
}

static PODModel pod_parse(const uint8_t* data, size_t size) {
    PODModel model;
    const uint8_t* p = data;
    const uint8_t* end = data + size;

    PODMesh* cur_mesh = nullptr;
    enum { CTX_NONE, CTX_FACES, CTX_VERTS, CTX_NORMS } data_ctx = CTX_NONE;
    int vd_type = 0, vd_n = 3, vd_stride = 0;
    
    std::vector<uint8_t> interleaved_buf;
    bool has_interleaved = false;
    int il_pos_off = -1, il_norm_off = -1;
    int il_pos_stride = 0, il_norm_stride = 0;

    while (p + 8 <= end) {
        uint32_t tag, length;
        if (!pod_read_u32(p, end, tag)) break;
        if (!pod_read_u32(p, end, length)) break;
        const uint8_t* cd = p;
        if (cd + length > end) length = (uint32_t)(end - cd);

        // End-tag (bit 31 set)
        if (tag & 0x80000000u) {
            uint32_t base = tag & 0x7FFFFFFF;
            if (base == 2012 && cur_mesh) { // end mesh
                std::cout << "[POD-END] nv=" << cur_mesh->num_vertices << " nf=" << cur_mesh->num_faces
                          << " il=" << has_interleaved << " buf=" << interleaved_buf.size()
                          << " pos_off=" << il_pos_off << " pos_stride=" << il_pos_stride
                          << " norm_off=" << il_norm_off << std::endl;
                if (has_interleaved && !interleaved_buf.empty()) {
                    int nv = cur_mesh->num_vertices;
                    // If stride is 0, compute from buffer: stride = buf_size / nv
                    int stride = il_pos_stride;
                    if (stride <= 0 && nv > 0)
                        stride = (int)(interleaved_buf.size() / nv);
                    int nstride = il_norm_stride;
                    if (nstride <= 0 && nv > 0)
                        nstride = stride; // normals use same stride in interleaved
                    
                    if (il_pos_off >= 0 && stride > 0 && nv > 0) {
                        cur_mesh->positions.resize(nv * 3);
                        for (int i = 0; i < nv; i++) {
                            int o = i * stride + il_pos_off;
                            if (o + 12 > (int)interleaved_buf.size()) break;
                            float x, y, z;
                            memcpy(&x, &interleaved_buf[o], 4);
                            memcpy(&y, &interleaved_buf[o+4], 4);
                            memcpy(&z, &interleaved_buf[o+8], 4);
                            cur_mesh->positions[i*3]=x; cur_mesh->positions[i*3+1]=y; cur_mesh->positions[i*3+2]=z;
                            if(x<cur_mesh->min_x) cur_mesh->min_x=x; if(y<cur_mesh->min_y) cur_mesh->min_y=y; if(z<cur_mesh->min_z) cur_mesh->min_z=z;
                            if(x>cur_mesh->max_x) cur_mesh->max_x=x; if(y>cur_mesh->max_y) cur_mesh->max_y=y; if(z>cur_mesh->max_z) cur_mesh->max_z=z;
                        }
                    }
                    if (il_norm_off >= 0 && nstride > 0 && nv > 0) {
                        cur_mesh->normals.resize(nv * 3);
                        for (int i = 0; i < nv; i++) {
                            int o = i * nstride + il_norm_off;
                            if (o + 12 > (int)interleaved_buf.size()) break;
                            memcpy(&cur_mesh->normals[i*3], &interleaved_buf[o], 12);
                        }
                    }
                    // Extract UVs: at offset 24 (pos=12 + norm=12) with float2 = 8 bytes
                    int uv_off = 24; // after float3 pos + float3 norm
                    if (stride >= 32 && nv > 0) { // stride must fit at least pos+norm+uv
                        cur_mesh->uvs.resize(nv * 2);
                        for (int i = 0; i < nv; i++) {
                            int o = i * stride + uv_off;
                            if (o + 8 > (int)interleaved_buf.size()) break;
                            float u, v;
                            memcpy(&u, &interleaved_buf[o], 4);
                            memcpy(&v, &interleaved_buf[o+4], 4);
                            cur_mesh->uvs[i*2] = u;
                            cur_mesh->uvs[i*2+1] = v;
                        }
                    }
                }
                cur_mesh = nullptr; data_ctx = CTX_NONE; has_interleaved = false;
                interleaved_buf.clear(); il_pos_off = il_norm_off = -1;
            }
            if (base == 6007 || base == 6008 || base == 6009 || base == 6012)
                data_ctx = CTX_NONE;
            p = cd + length;
            continue;
        }

        switch (tag) {
        case 1000: // Version
            if (length > 0 && length < 128) model.version.assign((const char*)cd, length);
            break;
        case 2004: // NumMesh
            if (length >= 4) { uint32_t n; memcpy(&n, cd, 4); model.meshes.reserve(n); }
            break;
        case 2009: // NumFrame
            if (length >= 4) memcpy(&model.num_frames, cd, 4);
            break;
        case 2012: // Mesh container start
            model.meshes.emplace_back();
            cur_mesh = &model.meshes.back();
            data_ctx = CTX_NONE; has_interleaved = false;
            interleaved_buf.clear(); il_pos_off = il_norm_off = -1;
            il_pos_stride = il_norm_stride = 0;
            break;

        // Mesh-level tags (6000-series)
        case 6000: // NumVertices
            if (cur_mesh && length >= 4) { uint32_t v; memcpy(&v, cd, 4); cur_mesh->num_vertices = (int)v; }
            break;
        case 6001: // NumFaces
            if (cur_mesh && length >= 4) { uint32_t v; memcpy(&v, cd, 4); cur_mesh->num_faces = (int)v; }
            break;
        case 6006: // Interleaved data (standard tag)
        case 6014: // Interleaved data (Swordigo's SDK version)
            if (cur_mesh && length > 0) {
                interleaved_buf.assign(cd, cd + length);
                has_interleaved = true;
                std::cout << "[POD-IL] tag=" << tag << " buf=" << length << " bytes" << std::endl;
            }
            break;
        // CPODData container tags (length=0)
        // CPODData container tags (confirmed from hex dump)
        case 6007: data_ctx = CTX_FACES; vd_type=0; vd_n=3; vd_stride=0; break;
        case 6008: data_ctx = CTX_VERTS; vd_type=0; vd_n=3; vd_stride=0; break;
        case 6009: data_ctx = CTX_NORMS; vd_type=0; vd_n=3; vd_stride=0; break;
        
        // Data descriptor tags (9000-series)
        case 9000: if (length >= 4) { uint32_t v; memcpy(&v, cd, 4); vd_type=(int)v; } break;
        case 9001: if (length >= 4) { uint32_t v; memcpy(&v, cd, 4); vd_n=(int)v; } break;
        case 9002: if (length >= 4) { uint32_t v; memcpy(&v, cd, 4); vd_stride=(int)v; } break;
        case 9003: // Data payload or interleaved offset
            if (!cur_mesh || length == 0) break;
            // Interleaved: offset only (4 bytes) for non-face data
            if (has_interleaved && data_ctx != CTX_FACES && length == 4) {
                uint32_t off; memcpy(&off, cd, 4);
                if (data_ctx == CTX_VERTS) {
                    il_pos_off = (int)off; il_pos_stride = vd_stride;
                    std::cout << "[POD-VTX] pos_off=" << off << " stride=" << vd_stride << " type=" << vd_type << " n=" << vd_n << std::endl;
                }
                else if (data_ctx == CTX_NORMS) {
                    il_norm_off = (int)off; il_norm_stride = vd_stride;
                    std::cout << "[POD-NRM] norm_off=" << off << " stride=" << vd_stride << std::endl;
                }
                break;
            }
            // Non-interleaved vertex positions
            if (data_ctx == CTX_VERTS && vd_type == 0 && vd_n >= 3) {
                int stride = vd_stride > 0 ? vd_stride : (int)(vd_n * 4);
                int cnt = cur_mesh->num_vertices > 0 ? cur_mesh->num_vertices : (int)(length / stride);
                cur_mesh->positions.resize(cnt * 3);
                for (int i = 0; i < cnt && (i*stride+12) <= (int)length; i++) {
                    float x,y,z;
                    memcpy(&x, cd+i*stride, 4); memcpy(&y, cd+i*stride+4, 4); memcpy(&z, cd+i*stride+8, 4);
                    cur_mesh->positions[i*3]=x; cur_mesh->positions[i*3+1]=y; cur_mesh->positions[i*3+2]=z;
                    if(x<cur_mesh->min_x) cur_mesh->min_x=x; if(y<cur_mesh->min_y) cur_mesh->min_y=y; if(z<cur_mesh->min_z) cur_mesh->min_z=z;
                    if(x>cur_mesh->max_x) cur_mesh->max_x=x; if(y>cur_mesh->max_y) cur_mesh->max_y=y; if(z>cur_mesh->max_z) cur_mesh->max_z=z;
                }
            }
            // Non-interleaved normals
            else if (data_ctx == CTX_NORMS && vd_type == 0 && vd_n >= 3) {
                int stride = vd_stride > 0 ? vd_stride : (int)(vd_n * 4);
                int cnt = cur_mesh->num_vertices > 0 ? cur_mesh->num_vertices : (int)(length / stride);
                cur_mesh->normals.resize(cnt * 3);
                for (int i = 0; i < cnt && (i*stride+12) <= (int)length; i++)
                    memcpy(&cur_mesh->normals[i*3], cd+i*stride, 12);
            }
            // Face indices (inside CTX_FACES container)
            else if (data_ctx == CTX_FACES && length > 4) {
                std::cout << "[POD-IDX] ctx=FACES type=" << vd_type << " len=" << length << " nf=" << cur_mesh->num_faces << std::endl;
                int cnt = cur_mesh->num_faces * 3;
                if (cnt > 0 && cnt * 2 <= (int)length) {
                    cur_mesh->indices.resize(cnt);
                    for (int i = 0; i < cnt && (i*2+2) <= (int)length; i++) {
                        uint16_t idx; memcpy(&idx, cd+i*2, 2);
                        cur_mesh->indices[i] = idx;
                    }
                } else if (cnt > 0 && cnt * 4 <= (int)length) {
                    cur_mesh->indices.resize(cnt);
                    for (int i = 0; i < cnt && (i*4+4) <= (int)length; i++) {
                        uint32_t idx; memcpy(&idx, cd+i*4, 4);
                        cur_mesh->indices[i] = (uint16_t)idx;
                    }
                }
            }
            // Flat face data (CTX_NONE = before any container, large payload = raw indices)
            else if (data_ctx == CTX_NONE && cur_mesh->indices.empty() && length > 4) {
                int cnt = cur_mesh->num_faces * 3;
                std::cout << "[POD-IDX] ctx=FLAT type=" << vd_type << " len=" << length << " nf=" << cur_mesh->num_faces << " cnt=" << cnt << std::endl;
                if (cnt > 0 && cnt * 2 <= (int)length) {
                    cur_mesh->indices.resize(cnt);
                    for (int i = 0; i < cnt && (i*2+2) <= (int)length; i++) {
                        uint16_t idx; memcpy(&idx, cd+i*2, 2);
                        cur_mesh->indices[i] = idx;
                    }
                } else if (cnt > 0 && cnt * 4 <= (int)length) {
                    cur_mesh->indices.resize(cnt);
                    for (int i = 0; i < cnt && (i*4+4) <= (int)length; i++) {
                        uint32_t idx; memcpy(&idx, cd+i*4, 4);
                        cur_mesh->indices[i] = (uint16_t)idx;
                    }
                }
            }
            break;
        default:
            if (cur_mesh)
                std::cout << "[POD-DBG] mesh tag=" << tag << " (0x" << std::hex << tag << std::dec << ") len=" << length << (length==0?" [CTR]":"") << std::endl;
            break;
        }
        p = cd + length;
    }

    // Compute bounding box
    for (auto& mesh : model.meshes) {
        if (mesh.positions.empty()) continue;
        if (mesh.min_x < model.min_x) model.min_x = mesh.min_x;
        if (mesh.min_y < model.min_y) model.min_y = mesh.min_y;
        if (mesh.min_z < model.min_z) model.min_z = mesh.min_z;
        if (mesh.max_x > model.max_x) model.max_x = mesh.max_x;
        if (mesh.max_y > model.max_y) model.max_y = mesh.max_y;
        if (mesh.max_z > model.max_z) model.max_z = mesh.max_z;
        model.total_vertices += (int)(mesh.positions.size() / 3);
        model.total_faces += (int)(mesh.indices.size() / 3);
    }
    model.center_x = (model.min_x + model.max_x) * 0.5f;
    model.center_y = (model.min_y + model.max_y) * 0.5f;
    model.center_z = (model.min_z + model.max_z) * 0.5f;
    float dx = model.max_x - model.min_x;
    float dy = model.max_y - model.min_y;
    float dz = model.max_z - model.min_z;
    model.radius = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
    if (model.radius < 0.001f) model.radius = 1.0f;
    return model;
}

static PODModel load_pod_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return PODModel{};
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(sz);
    f.read((char*)data.data(), sz);
    f.close();
    return pod_parse(data.data(), data.size());
}

// ============================================================================
// Application State
// ============================================================================

struct AppState {
    // Window
    SDL_Window*   window   = nullptr;
    SDL_GLContext  gl_ctx   = nullptr;
    int win_w = WIN_W, win_h = WIN_H;
    bool running = true;

    // Current directory
    std::string current_dir;
    std::vector<FileEntry> entries;
    std::vector<FileEntry> filtered_entries;
    FilterMode filter = FILTER_ALL;

    // Selection
    int selected_index = -1;
    int hover_index    = -1;

    // Scroll state (file list)
    float scroll_y     = 0.0f;
    float max_scroll_y = 0.0f;
    bool  scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;

    // Preview texture
    GLuint preview_tex = 0;
    int    preview_w   = 0;
    int    preview_h   = 0;
    std::string preview_name;
    std::string preview_info; // multi-line info string

    // 3D Model preview
    PODModel model;
    bool model_loaded = false;
    float cam_yaw   = 30.0f;   // degrees
    float cam_pitch = 20.0f;   // degrees  
    float cam_dist  = 3.0f;    // distance multiplier (of model radius)
    bool  cam_dragging = false;
    int   cam_drag_x = 0, cam_drag_y = 0;
    float cam_drag_yaw = 0, cam_drag_pitch = 0;
    bool  wireframe_mode = false;
    GLuint model_tex = 0;   // texture applied to 3D model
    bool   tex_mode = false; // T key: show texture on model

    // Texture preview zoom/pan
    float tex_zoom = 1.0f;
    float tex_pan_x = 0.0f, tex_pan_y = 0.0f;
    bool  tex_dragging = false;
    int   tex_drag_x = 0, tex_drag_y = 0;
    float tex_drag_pan_x = 0.0f, tex_drag_pan_y = 0.0f;

    // Mouse
    int mouse_x = 0, mouse_y = 0;
    bool mouse_down = false;
    bool mouse_clicked = false; // single-frame click
    bool right_mouse_down = false;
};

static AppState app;

// ============================================================================
// Expand ~ in paths
// ============================================================================

static std::string expand_home(const std::string& path) {
    if (!path.empty() && path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) return std::string(home) + path.substr(1);
    }
    return path;
}

// ============================================================================
// Format file size
// ============================================================================

static std::string format_size(uintmax_t bytes) {
    char buf[64];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%ju B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    else
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

// ============================================================================
// Load directory listing
// ============================================================================

static void load_directory(const std::string& path) {
    app.entries.clear();
    app.selected_index = -1;
    app.hover_index = -1;
    app.scroll_y = 0.0f;

    std::string dir = expand_home(path);

    // Normalize path
    std::error_code ec;
    auto canonical = fs::canonical(dir, ec);
    if (ec) {
        std::cerr << "[AV] Cannot access: " << dir << " (" << ec.message() << ")" << std::endl;
        app.current_dir = dir;
        return;
    }
    app.current_dir = canonical.string();

    // Add parent directory entry (unless at root)
    if (app.current_dir != "/") {
        FileEntry parent;
        parent.name = "..";
        parent.full_path = fs::path(app.current_dir).parent_path().string();
        parent.type = FT_DIRECTORY;
        parent.size = 0;
        parent.is_dir = true;
        app.entries.push_back(parent);
    }

    // Scan directory
    try {
        std::vector<FileEntry> dirs, files;
        for (auto& entry : fs::directory_iterator(app.current_dir, fs::directory_options::skip_permission_denied)) {
            FileEntry fe;
            fe.name = entry.path().filename().string();
            fe.full_path = entry.path().string();
            fe.is_dir = entry.is_directory();
            fe.type = classify_file(fe.name, fe.is_dir);
            fe.size = fe.is_dir ? 0 : entry.file_size(ec);

            if (fe.is_dir)
                dirs.push_back(fe);
            else
                files.push_back(fe);
        }
        // Sort directories and files alphabetically (case-insensitive)
        auto icmp = [](const FileEntry& a, const FileEntry& b) {
            std::string la = a.name, lb = b.name;
            for (auto& c : la) c = tolower(c);
            for (auto& c : lb) c = tolower(c);
            return la < lb;
        };
        std::sort(dirs.begin(), dirs.end(), icmp);
        std::sort(files.begin(), files.end(), icmp);
        app.entries.insert(app.entries.end(), dirs.begin(), dirs.end());
        app.entries.insert(app.entries.end(), files.begin(), files.end());
    } catch (const std::exception& e) {
        std::cerr << "[AV] Directory scan error: " << e.what() << std::endl;
    }

    // Apply filter
    app.filtered_entries.clear();
    for (auto& e : app.entries) {
        if (passes_filter(e.type, app.filter))
            app.filtered_entries.push_back(e);
    }

    std::cout << "[AV] Loaded: " << app.current_dir
              << " (" << app.filtered_entries.size() << " items)" << std::endl;
}

static void apply_filter() {
    app.filtered_entries.clear();
    for (auto& e : app.entries) {
        if (passes_filter(e.type, app.filter))
            app.filtered_entries.push_back(e);
    }
    app.selected_index = -1;
    app.hover_index = -1;
    app.scroll_y = 0.0f;
}

// ============================================================================
// Clear preview
// ============================================================================

static void clear_preview() {
    if (app.preview_tex) {
        glDeleteTextures(1, &app.preview_tex);
        app.preview_tex = 0;
    }
    if (app.model_tex) {
        glDeleteTextures(1, &app.model_tex);
        app.model_tex = 0;
    }
    app.preview_w = 0;
    app.preview_h = 0;
    app.preview_name.clear();
    app.preview_info.clear();
    app.model = PODModel{};
    app.model_loaded = false;
    app.tex_mode = false;
    app.tex_zoom = 1.0f;
    app.tex_pan_x = app.tex_pan_y = 0.0f;
}

// ============================================================================
// Load PNG texture via SDL3_image
// ============================================================================

static GLuint load_png_texture(const char* path, int* out_w, int* out_h) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        std::cerr << "[AV] IMG_Load failed: " << path << " — " << SDL_GetError() << std::endl;
        return 0;
    }

    // Convert to RGBA32 if needed
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!rgba) {
        std::cerr << "[AV] Surface convert failed" << std::endl;
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    SDL_LockSurface(rgba);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    SDL_UnlockSurface(rgba);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (out_w) *out_w = rgba->w;
    if (out_h) *out_h = rgba->h;

    std::cout << "[AV] Loaded PNG: " << rgba->w << "x" << rgba->h << std::endl;

    SDL_DestroySurface(rgba);
    return tex;
}

// ============================================================================
// Preview a file
// ============================================================================

static void preview_file(const FileEntry& fe) {
    clear_preview();
    app.preview_name = fe.name;

    char info[512];
    snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: %s",
             fe.name.c_str(), format_size(fe.size).c_str(), filetype_label(fe.type));

    if (fe.type == FT_TEXTURE_PVR) {
        int w = 0, h = 0;
        GLuint tex = pvr_load_texture(fe.full_path.c_str(), &w, &h);
        if (tex) {
            app.preview_tex = tex;
            app.preview_w = w;
            app.preview_h = h;
            snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: PVR (ETC1)\nDimensions: %dx%d",
                     fe.name.c_str(), format_size(fe.size).c_str(), w, h);
        } else {
            snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: PVR\nError: Failed to decode",
                     fe.name.c_str(), format_size(fe.size).c_str());
        }
    }
    else if (fe.type == FT_TEXTURE_PNG || fe.type == FT_TEXTURE_JPG) {
        int w = 0, h = 0;
        GLuint tex = load_png_texture(fe.full_path.c_str(), &w, &h);
        if (tex) {
            app.preview_tex = tex;
            app.preview_w = w;
            app.preview_h = h;
            snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: %s\nDimensions: %dx%d",
                     fe.name.c_str(), format_size(fe.size).c_str(),
                     fe.type == FT_TEXTURE_PNG ? "PNG" : "JPEG", w, h);
        }
    }
    else if (fe.type == FT_AUDIO_WAV || fe.type == FT_AUDIO_OGG) {
        snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: %s Audio\n(Preview not implemented)",
                 fe.name.c_str(), format_size(fe.size).c_str(),
                 fe.type == FT_AUDIO_WAV ? "WAV" : "OGG Vorbis");
    }
    else if (fe.type == FT_MODEL_POD) {
        app.model = load_pod_file(fe.full_path);
        if (!app.model.meshes.empty()) {
            app.model_loaded = true;
            app.cam_yaw = 30.0f;
            app.cam_pitch = 20.0f;
            app.cam_dist = 3.0f;
            snprintf(info, sizeof(info),
                     "File: %s\nSize: %s\nType: POD 3D Model\n"
                     "Meshes: %zu  Verts: %d  Tris: %d\n"
                     "Version: %s  Frames: %d",
                     fe.name.c_str(), format_size(fe.size).c_str(),
                     app.model.meshes.size(), app.model.total_vertices,
                     app.model.total_faces, app.model.version.c_str(),
                     app.model.num_frames);
            std::cout << "[AV] POD: " << app.model.meshes.size() << " meshes, "
                      << app.model.total_vertices << " verts, "
                      << app.model.total_faces << " tris" << std::endl;
            // Auto-load matching PVR texture (same basename)
            std::string base = fs::path(fe.full_path).stem().string();
            std::string pvr_path = fs::path(fe.full_path).parent_path().string() + "/" + base + ".pvr";
            int tw = 0, th = 0;
            GLuint mtex = pvr_load_texture(pvr_path.c_str(), &tw, &th);
            if (mtex) {
                app.model_tex = mtex;
                app.tex_mode = true;
                std::cout << "[AV] Loaded texture: " << base << ".pvr (" << tw << "x" << th << ")" << std::endl;
            }
        } else {
            snprintf(info, sizeof(info),
                     "File: %s\nSize: %s\nType: POD 3D Model\nError: No mesh data found",
                     fe.name.c_str(), format_size(fe.size).c_str());
        }
    }
    else if (fe.type == FT_SCENE) {
        snprintf(info, sizeof(info), "File: %s\nSize: %s\nType: Scene Definition\n(Scene preview not implemented)",
                 fe.name.c_str(), format_size(fe.size).c_str());
    }

    app.preview_info = info;
}

// ============================================================================
// Handle file selection (click)
// ============================================================================

static void on_file_selected(int index) {
    if (index < 0 || index >= (int)app.filtered_entries.size()) return;
    const FileEntry& fe = app.filtered_entries[index];

    if (fe.is_dir) {
        // Navigate into directory
        load_directory(fe.full_path);
        clear_preview();
    } else {
        app.selected_index = index;
        preview_file(fe);
    }
}

// ============================================================================
// Render: Top bar (current path)
// ============================================================================

static void render_top_bar() {
    float w = (float)app.win_w;
    draw_rect(0, 0, w, TOP_BAR_H, BAR_R, BAR_G, BAR_B);

    // Path breadcrumb
    std::string display_path = app.current_dir;
    // Truncate if too long
    float max_path_w = w - 20.0f;
    float pw = text_width(display_path, SMALL_SCALE);
    if (pw > max_path_w) {
        // Show ".../<tail>"
        while (display_path.length() > 3 && text_width("..." + display_path, SMALL_SCALE) > max_path_w) {
            auto slash = display_path.find('/', 1);
            if (slash == std::string::npos) break;
            display_path = display_path.substr(slash);
        }
        display_path = "..." + display_path;
    }

    draw_string(display_path, 10.0f, 10.0f, SMALL_SCALE, ACCENT_R, ACCENT_G, ACCENT_B);

    // Bottom border
    draw_rect(0, TOP_BAR_H - 1, w, 1, BORDER_R, BORDER_G, BORDER_B);
}

// ============================================================================
// Render: Filter bar
// ============================================================================

static void render_filter_bar() {
    float y = TOP_BAR_H;
    float w = LEFT_PANEL_W;
    draw_rect(0, y, w, FILTER_BAR_H, PANEL_R, PANEL_G, PANEL_B);

    float btn_x = 6.0f;
    float btn_y = y + 5.0f;
    float btn_h = FILTER_BAR_H - 10.0f;

    for (int i = 0; i < FILTER_COUNT; i++) {
        float btn_w = text_width(filter_names[i], SMALL_SCALE) + 16.0f;

        // Hover check
        bool hovered = (app.mouse_x >= (int)btn_x && app.mouse_x < (int)(btn_x + btn_w) &&
                        app.mouse_y >= (int)btn_y && app.mouse_y < (int)(btn_y + btn_h));

        if (app.filter == (FilterMode)i) {
            draw_rect(btn_x, btn_y, btn_w, btn_h, SEL_R, SEL_G, SEL_B);
        } else if (hovered) {
            draw_rect(btn_x, btn_y, btn_w, btn_h, HOVER_R, HOVER_G, HOVER_B);
        }

        draw_string(filter_names[i], btn_x + 8.0f, btn_y + 4.0f, SMALL_SCALE,
                     TEXT_R, TEXT_G, TEXT_B);

        // Click handling
        if (hovered && app.mouse_clicked) {
            app.filter = (FilterMode)i;
            apply_filter();
        }

        btn_x += btn_w + 4.0f;
    }

    // Bottom border
    draw_rect(0, y + FILTER_BAR_H - 1, w, 1, BORDER_R, BORDER_G, BORDER_B);
}

// ============================================================================
// Render: File list (left panel)
// ============================================================================

static void render_file_list() {
    float panel_x = 0;
    float panel_y = TOP_BAR_H + FILTER_BAR_H;
    float panel_w = LEFT_PANEL_W;
    float panel_h = (float)app.win_h - panel_y;

    // Background
    draw_rect(panel_x, panel_y, panel_w, panel_h, PANEL_R, PANEL_G, PANEL_B);

    // Calculate scroll limits
    float total_h = app.filtered_entries.size() * FILE_ROW_H;
    app.max_scroll_y = std::max(0.0f, total_h - panel_h);
    app.scroll_y = std::clamp(app.scroll_y, 0.0f, app.max_scroll_y);

    // Set scissor for file list area
    // Note: glScissor uses bottom-left origin, so we flip Y
    int sc_y = app.win_h - (int)(panel_y + panel_h);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)panel_x, sc_y, (int)panel_w, (int)panel_h);

    // Update hover
    app.hover_index = -1;

    for (int i = 0; i < (int)app.filtered_entries.size(); i++) {
        float row_y = panel_y + i * FILE_ROW_H - app.scroll_y;

        // Skip if off-screen
        if (row_y + FILE_ROW_H < panel_y || row_y > panel_y + panel_h)
            continue;

        const FileEntry& fe = app.filtered_entries[i];

        // Hover detection
        bool hovered = (app.mouse_x >= (int)panel_x &&
                        app.mouse_x < (int)(panel_x + panel_w - SCROLLBAR_W) &&
                        app.mouse_y >= (int)row_y && app.mouse_y < (int)(row_y + FILE_ROW_H) &&
                        app.mouse_y >= (int)panel_y && app.mouse_y < (int)(panel_y + panel_h));

        if (hovered) app.hover_index = i;

        // Selection / hover highlight
        if (i == app.selected_index) {
            draw_rect(panel_x, row_y, panel_w - SCROLLBAR_W, FILE_ROW_H, SEL_R, SEL_G, SEL_B, 180);
        } else if (hovered) {
            draw_rect(panel_x, row_y, panel_w - SCROLLBAR_W, FILE_ROW_H, HOVER_R, HOVER_G, HOVER_B, 120);
        }

        // Type tag color
        uint8_t tr, tg, tb;
        filetype_color(fe.type, tr, tg, tb);

        // Type tag
        char tag[8];
        snprintf(tag, sizeof(tag), "%-5s", filetype_label(fe.type));
        draw_string(tag, panel_x + 6.0f, row_y + 4.0f, SMALL_SCALE, tr, tg, tb);

        // Filename
        float name_x = panel_x + 58.0f;
        float max_name_w = panel_w - 70.0f - SCROLLBAR_W;
        std::string display_name = fe.name;
        // Truncate if too long
        while (text_width(display_name, SMALL_SCALE) > max_name_w && display_name.length() > 3) {
            display_name = display_name.substr(0, display_name.length() - 4) + "...";
        }

        if (fe.is_dir) {
            draw_string(display_name, name_x, row_y + 4.0f, SMALL_SCALE, DIR_R, DIR_G, DIR_B);
        } else {
            draw_string(display_name, name_x, row_y + 4.0f, SMALL_SCALE, TEXT_R, TEXT_G, TEXT_B);
        }

        // Click handling
        if (hovered && app.mouse_clicked) {
            on_file_selected(i);
        }
    }

    glDisable(GL_SCISSOR_TEST);

    // Scrollbar
    if (app.max_scroll_y > 0) {
        float sb_x = panel_x + panel_w - SCROLLBAR_W;
        float sb_track_h = panel_h;
        float sb_thumb_h = std::max(20.0f, (panel_h / total_h) * sb_track_h);
        float sb_thumb_y = panel_y + (app.scroll_y / app.max_scroll_y) * (sb_track_h - sb_thumb_h);

        // Track
        draw_rect(sb_x, panel_y, SCROLLBAR_W, sb_track_h, 22, 22, 30);
        // Thumb
        bool sb_hovered = (app.mouse_x >= (int)sb_x && app.mouse_x < (int)(sb_x + SCROLLBAR_W) &&
                           app.mouse_y >= (int)sb_thumb_y && app.mouse_y < (int)(sb_thumb_y + sb_thumb_h));
        if (app.scrollbar_dragging || sb_hovered)
            draw_rect(sb_x + 2, sb_thumb_y, SCROLLBAR_W - 4, sb_thumb_h, 80, 80, 100);
        else
            draw_rect(sb_x + 2, sb_thumb_y, SCROLLBAR_W - 4, sb_thumb_h, 55, 55, 70);

        // Scrollbar drag logic
        if (sb_hovered && app.mouse_clicked) {
            app.scrollbar_dragging = true;
            app.scrollbar_drag_offset = app.mouse_y - sb_thumb_y;
        }
        if (app.scrollbar_dragging) {
            if (app.mouse_down) {
                float new_thumb_y = app.mouse_y - app.scrollbar_drag_offset;
                float ratio = (new_thumb_y - panel_y) / (sb_track_h - sb_thumb_h);
                app.scroll_y = std::clamp(ratio * app.max_scroll_y, 0.0f, app.max_scroll_y);
            } else {
                app.scrollbar_dragging = false;
            }
        }
    }

    // Right border of left panel
    draw_rect(panel_w - 1, TOP_BAR_H, 1, (float)app.win_h - TOP_BAR_H, BORDER_R, BORDER_G, BORDER_B);
}

// ============================================================================
// Render: Preview panel (right side)
// ============================================================================

// Helper: render 3D model with orbit camera
static void render_3d_model(float vp_x, float vp_y, float vp_w, float vp_h) {
    // Save GL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Set viewport for the 3D region (GL viewport uses bottom-left origin)
    int gl_vp_y = app.win_h - (int)(vp_y + vp_h);
    glViewport((int)vp_x, gl_vp_y, (int)vp_w, (int)vp_h);
    
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    
    // Perspective projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    float aspect = vp_w / vp_h;
    float fov = 45.0f;
    float near_p = app.model.radius * 0.01f;
    float far_p = app.model.radius * 20.0f;
    float top = near_p * tanf(fov * 0.5f * 3.14159f / 180.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, near_p, far_p);
    
    // Camera transform (orbit)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    float dist = app.model.radius * app.cam_dist;
    float pitch_rad = app.cam_pitch * 3.14159f / 180.0f;
    float yaw_rad = app.cam_yaw * 3.14159f / 180.0f;
    float eye_x = app.model.center_x + dist * cosf(pitch_rad) * sinf(yaw_rad);
    float eye_y = app.model.center_y + dist * sinf(pitch_rad);
    float eye_z = app.model.center_z + dist * cosf(pitch_rad) * cosf(yaw_rad);
    
    // gluLookAt equivalent via manual matrix
    float fx = app.model.center_x - eye_x;
    float fy = app.model.center_y - eye_y;
    float fz = app.model.center_z - eye_z;
    float flen = sqrtf(fx*fx + fy*fy + fz*fz);
    if (flen > 0) { fx /= flen; fy /= flen; fz /= flen; }
    // Up = (0,1,0)
    float sx = fy * 0.0f - fz * 1.0f; // wait, up=(0,1,0): s = f x up
    // Actually: s = f × up
    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    float slen = sqrtf(sx*sx + sy*sy + sz*sz);
    if (slen > 0) { sx /= slen; sy /= slen; sz /= slen; }
    // u = s × f
    ux = sy * fz - sz * fy;
    uy = sz * fx - sx * fz;
    uz = sx * fy - sy * fx;
    
    float m[16] = {
        sx, ux, -fx, 0,
        sy, uy, -fy, 0,
        sz, uz, -fz, 0,
        -(sx*eye_x + sy*eye_y + sz*eye_z),
        -(ux*eye_x + uy*eye_y + uz*eye_z),
        -(-fx*eye_x + -fy*eye_y + -fz*eye_z),
        1
    };
    glLoadMatrixf(m);
    
    // Enable lighting for solid mode
    if (!app.wireframe_mode) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        float light_pos[] = { eye_x, eye_y + app.model.radius, eye_z, 1.0f };
        float light_diff[] = { 0.85f, 0.85f, 0.95f, 1.0f };
        float light_amb[] = { 0.2f, 0.2f, 0.25f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    }
    // Bind model texture if available
    bool use_tex = app.tex_mode && app.model_tex && !app.wireframe_mode;
    if (use_tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, app.model_tex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3f(1.0f, 1.0f, 1.0f); // white base color for modulate
    }
    
    // Draw each mesh
    for (size_t mi = 0; mi < app.model.meshes.size(); mi++) {
        const auto& mesh = app.model.meshes[mi];
        if (mesh.positions.empty()) continue;
        
        if (!use_tex) {
            // Color per mesh (cycle through a palette)
            uint8_t colors[][3] = {
                {120, 180, 255}, {255, 160, 100}, {100, 220, 140},
                {220, 130, 255}, {255, 220, 80},  {100, 220, 220},
                {255, 130, 130}, {180, 180, 220}
            };
            auto& c = colors[mi % 8];
            glColor3ub(c[0], c[1], c[2]);
        }
        
        if (app.wireframe_mode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.0f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, mesh.positions.data());
        if (!mesh.normals.empty() && !app.wireframe_mode) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, 0, mesh.normals.data());
        }
        if (use_tex && !mesh.uvs.empty()) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, mesh.uvs.data());
        }
        
        if (!mesh.indices.empty()) {
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(),
                           GL_UNSIGNED_SHORT, mesh.indices.data());
        } else if (mesh.positions.size() >= 9) {
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(mesh.positions.size() / 3));
        }
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    
    if (use_tex) {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    // Draw grid on XZ plane
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    float grid_size = app.model.radius * 2.0f;
    float grid_step = grid_size / 10.0f;
    glColor4ub(80, 80, 100, 100);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++) {
        float v = app.model.center_x + i * grid_step;
        float z0 = app.model.center_z - grid_size;
        float z1 = app.model.center_z + grid_size;
        glVertex3f(v, app.model.min_y, z0);
        glVertex3f(v, app.model.min_y, z1);
        float x0 = app.model.center_x - grid_size;
        float x1 = app.model.center_x + grid_size;
        glVertex3f(x0, app.model.min_y, v - app.model.center_x + app.model.center_z);
        glVertex3f(x1, app.model.min_y, v - app.model.center_x + app.model.center_z);
    }
    glEnd();
    
    // Restore
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

static void render_preview_panel() {
    float panel_x = LEFT_PANEL_W;
    float panel_y = TOP_BAR_H;
    float panel_w = (float)app.win_w - LEFT_PANEL_W;
    float panel_h = (float)app.win_h - TOP_BAR_H - INFO_BAR_H;

    // Background
    draw_rect(panel_x, panel_y, panel_w, panel_h, BG_R, BG_G, BG_B);

    if (app.model_loaded) {
        // 3D model preview
        render_3d_model(panel_x, panel_y, panel_w, panel_h);
        
        // After 3D, restore 2D projection for overlay text
        glViewport(0, 0, app.win_w, app.win_h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, app.win_w, app.win_h, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        
        // Controls hint
        float hint_w = app.model_tex ? 420.0f : 320.0f;
        draw_rect(panel_x + 8, panel_y + 4, hint_w, 22, 0, 0, 0, 160);
        draw_string(app.model_tex ? "Drag:Orbit  Wheel:Zoom  W:Wire  T:Texture" : "Drag:Orbit  Wheel:Zoom  W:Wireframe",
                     panel_x + 14, panel_y + 8, SMALL_SCALE, 160, 200, 255, 220);
        
        // Mode indicator
        if (app.wireframe_mode) {
            draw_rect(panel_x + panel_w - 110, panel_y + 4, 100, 22, 0, 0, 0, 160);
            draw_string("WIREFRAME", panel_x + panel_w - 100, panel_y + 8,
                         SMALL_SCALE, 100, 255, 160);
        } else if (app.tex_mode && app.model_tex) {
            draw_rect(panel_x + panel_w - 110, panel_y + 4, 100, 22, 0, 0, 0, 160);
            draw_string("TEXTURED", panel_x + panel_w - 100, panel_y + 8,
                         SMALL_SCALE, 255, 200, 80);
        }
    }
    else if (app.preview_tex) {
        // Fit texture to panel with zoom/pan
        float pad = 20.0f;
        float avail_w = panel_w - pad * 2;
        float avail_h = panel_h - pad * 2;

        float base_scale_w = avail_w / app.preview_w;
        float base_scale_h = avail_h / app.preview_h;
        float base_scale = std::min(base_scale_w, base_scale_h);
        if (base_scale > 1.0f) base_scale = 1.0f;
        
        float scale = base_scale * app.tex_zoom;

        float draw_w = app.preview_w * scale;
        float draw_h = app.preview_h * scale;
        float draw_x = panel_x + (panel_w - draw_w) / 2.0f + app.tex_pan_x;
        float draw_y = panel_y + (panel_h - draw_h) / 2.0f + app.tex_pan_y;

        // Checkerboard background for transparency
        float check_size = 16.0f;
        glEnable(GL_SCISSOR_TEST);
        int sc_y = app.win_h - (int)(draw_y + draw_h);
        glScissor((int)draw_x, sc_y, (int)draw_w, (int)draw_h);
        for (float cy = draw_y; cy < draw_y + draw_h; cy += check_size) {
            for (float cx = draw_x; cx < draw_x + draw_w; cx += check_size) {
                int ix = (int)((cx - draw_x) / check_size);
                int iy = (int)((cy - draw_y) / check_size);
                uint8_t v = ((ix + iy) % 2 == 0) ? 40 : 50;
                float cw = std::min(check_size, draw_x + draw_w - cx);
                float ch = std::min(check_size, draw_y + draw_h - cy);
                draw_rect(cx, cy, cw, ch, v, v, v);
            }
        }
        glDisable(GL_SCISSOR_TEST);

        // Draw the texture
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_textured_quad(app.preview_tex, draw_x, draw_y, draw_w, draw_h);

        // Border around image
        draw_border(draw_x, draw_y, draw_w, draw_h, BORDER_R, BORDER_G, BORDER_B);
        
        // Zoom controls hint
        char zoom_buf[64];
        snprintf(zoom_buf, sizeof(zoom_buf), "Wheel:Zoom(%.0f%%)  Drag:Pan  R:Reset", app.tex_zoom * 100.0f);
        draw_rect(panel_x + 8, panel_y + 4, 340, 22, 0, 0, 0, 160);
        draw_string(zoom_buf, panel_x + 14, panel_y + 8, SMALL_SCALE, 160, 200, 255, 220);
    }
    else if (!app.preview_name.empty()) {
        // No texture preview, show centered message
        const char* msg = "No preview available";
        float tw = text_width(msg, TEXT_SCALE);
        float tx = panel_x + (panel_w - tw) / 2.0f;
        float ty = panel_y + panel_h / 2.0f - 6.0f;
        draw_string(msg, tx, ty, TEXT_SCALE, DIM_R, DIM_G, DIM_B);
    }
    else {
        // Nothing selected
        const char* msg = "Select a file to preview";
        float tw = text_width(msg, TEXT_SCALE);
        float tx = panel_x + (panel_w - tw) / 2.0f;
        float ty = panel_y + panel_h / 2.0f - 6.0f;
        draw_string(msg, tx, ty, TEXT_SCALE, DIM_R, DIM_G, DIM_B, 150);
    }
}

// ============================================================================
// Render: Info bar (bottom right)
// ============================================================================

static void render_info_bar() {
    float bar_x = LEFT_PANEL_W;
    float bar_y = (float)app.win_h - INFO_BAR_H;
    float bar_w = (float)app.win_w - LEFT_PANEL_W;

    // Background
    draw_rect(bar_x, bar_y, bar_w, INFO_BAR_H, BAR_R, BAR_G, BAR_B);
    // Top border
    draw_rect(bar_x, bar_y, bar_w, 1, BORDER_R, BORDER_G, BORDER_B);

    if (!app.preview_info.empty()) {
        draw_string(app.preview_info, bar_x + 14.0f, bar_y + 10.0f, SMALL_SCALE,
                     TEXT_R, TEXT_G, TEXT_B);
    } else {
        // Show item count
        char buf[128];
        snprintf(buf, sizeof(buf), "%zu items", app.filtered_entries.size());
        draw_string(buf, bar_x + 14.0f, bar_y + 10.0f, SMALL_SCALE, DIM_R, DIM_G, DIM_B);
    }
}

// ============================================================================
// Render frame
// ============================================================================

static void render_frame() {
    glViewport(0, 0, app.win_w, app.win_h);
    glClearColor(BG_R / 255.0f, BG_G / 255.0f, BG_B / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up 2D projection (top-left origin)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, app.win_w, app.win_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    render_top_bar();
    render_filter_bar();
    render_file_list();
    render_preview_panel();
    render_info_bar();
}

// ============================================================================
// Event handling
// ============================================================================

static void handle_events() {
    app.mouse_clicked = false;
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                app.running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                app.win_w = ev.window.data1;
                app.win_h = ev.window.data2;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                app.mouse_x = (int)ev.motion.x;
                app.mouse_y = (int)ev.motion.y;
                // Orbit camera drag
                if (app.cam_dragging && app.model_loaded) {
                    float dx = (float)(app.mouse_x - app.cam_drag_x);
                    float dy = (float)(app.mouse_y - app.cam_drag_y);
                    app.cam_yaw = app.cam_drag_yaw + dx * 0.5f;
                    app.cam_pitch = std::clamp(app.cam_drag_pitch - dy * 0.5f, -89.0f, 89.0f);
                }
                // Texture pan drag
                if (app.tex_dragging && app.preview_tex) {
                    app.tex_pan_x = app.tex_drag_pan_x + (float)(app.mouse_x - app.tex_drag_x);
                    app.tex_pan_y = app.tex_drag_pan_y + (float)(app.mouse_y - app.tex_drag_y);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    app.mouse_down = true;
                    app.mouse_x = (int)ev.button.x;
                    app.mouse_y = (int)ev.button.y;
                    // Start orbit drag if clicking on the 3D preview area
                    if (app.model_loaded && app.mouse_x > (int)LEFT_PANEL_W
                        && app.mouse_y > (int)TOP_BAR_H
                        && app.mouse_y < app.win_h - (int)INFO_BAR_H) {
                        app.cam_dragging = true;
                        app.cam_drag_x = app.mouse_x;
                        app.cam_drag_y = app.mouse_y;
                        app.cam_drag_yaw = app.cam_yaw;
                        app.cam_drag_pitch = app.cam_pitch;
                    }
                    // PVR texture drag-pan
                    else if (app.preview_tex && !app.model_loaded) {
                        app.tex_dragging = true;
                        app.tex_drag_x = app.mouse_x;
                        app.tex_drag_y = app.mouse_y;
                        app.tex_drag_pan_x = app.tex_pan_x;
                        app.tex_drag_pan_y = app.tex_pan_y;
                    } else {
                        app.mouse_clicked = true;
                    }
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    app.mouse_down = false;
                    app.cam_dragging = false;
                    app.tex_dragging = false;
                    app.scrollbar_dragging = false;
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
            {
                // Zoom 3D preview if mouse is over right panel
                if (app.model_loaded && app.mouse_x > (int)LEFT_PANEL_W) {
                    app.cam_dist -= ev.wheel.y * 0.3f;
                    app.cam_dist = std::clamp(app.cam_dist, 0.5f, 20.0f);
                }
                // Zoom PVR texture preview
                else if (app.preview_tex && !app.model_loaded && app.mouse_x > (int)LEFT_PANEL_W) {
                    app.tex_zoom *= (ev.wheel.y > 0) ? 1.15f : (1.0f / 1.15f);
                    app.tex_zoom = std::clamp(app.tex_zoom, 0.25f, 16.0f);
                }
                // Scroll file list if mouse is over left panel
                else if (app.mouse_x < (int)LEFT_PANEL_W) {
                    float panel_y = TOP_BAR_H + FILTER_BAR_H;
                    if (app.mouse_y > (int)panel_y) {
                        app.scroll_y -= ev.wheel.y * FILE_ROW_H * 3.0f;
                        app.scroll_y = std::clamp(app.scroll_y, 0.0f, app.max_scroll_y);
                    }
                }
                break;
            }

            case SDL_EVENT_KEY_DOWN:
                switch (ev.key.key) {
                    case SDLK_ESCAPE:
                        app.running = false;
                        break;
                    case SDLK_BACKSPACE:
                        // Go up one directory
                        if (app.current_dir != "/") {
                            std::string parent = fs::path(app.current_dir).parent_path().string();
                            load_directory(parent);
                            clear_preview();
                        }
                        break;
                    case SDLK_W:
                        // Toggle wireframe mode
                        if (app.model_loaded) {
                            app.wireframe_mode = !app.wireframe_mode;
                        }
                        break;
                    case SDLK_T:
                        // Toggle texture on 3D model
                        if (app.model_loaded && app.model_tex) {
                            app.tex_mode = !app.tex_mode;
                        }
                        break;
                    case SDLK_R:
                        // Reset texture zoom/pan
                        app.tex_zoom = 1.0f;
                        app.tex_pan_x = app.tex_pan_y = 0.0f;
                        break;
                    case SDLK_UP:
                        if (app.selected_index > 0) {
                            app.selected_index--;
                            // Auto-scroll
                            float row_y = TOP_BAR_H + FILTER_BAR_H + app.selected_index * FILE_ROW_H - app.scroll_y;
                            if (row_y < TOP_BAR_H + FILTER_BAR_H)
                                app.scroll_y -= FILE_ROW_H;
                        }
                        break;
                    case SDLK_DOWN:
                        if (app.selected_index < (int)app.filtered_entries.size() - 1) {
                            app.selected_index++;
                            float row_y = TOP_BAR_H + FILTER_BAR_H + app.selected_index * FILE_ROW_H - app.scroll_y;
                            float panel_bottom = (float)app.win_h;
                            if (row_y + FILE_ROW_H > panel_bottom)
                                app.scroll_y += FILE_ROW_H;
                        }
                        break;
                    case SDLK_RETURN:
                        on_file_selected(app.selected_index);
                        break;
                    case SDLK_HOME:
                        app.scroll_y = 0;
                        break;
                    case SDLK_END:
                        app.scroll_y = app.max_scroll_y;
                        break;
                    case SDLK_F1: app.filter = FILTER_ALL;      apply_filter(); break;
                    case SDLK_F2: app.filter = FILTER_TEXTURES; apply_filter(); break;
                    case SDLK_F3: app.filter = FILTER_MODELS;   apply_filter(); break;
                    case SDLK_F4: app.filter = FILTER_SCENES;   apply_filter(); break;
                    case SDLK_F5: app.filter = FILTER_AUDIO;    apply_filter(); break;
                    default: break;
                }
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// Find a valid starting directory
// ============================================================================

static std::string find_start_dir(const char* arg) {
    // 1. Command-line argument
    if (arg) {
        std::string expanded = expand_home(arg);
        if (fs::is_directory(expanded)) return expanded;
    }

    // 2. Default asset directories
    for (auto& d : DEFAULT_DIRS) {
        std::string expanded = expand_home(d);
        if (fs::is_directory(expanded)) return expanded;
    }

    // 3. Home directory
    const char* home = getenv("HOME");
    if (home && fs::is_directory(home)) return home;

    // 4. Current directory
    return ".";
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Swordigo Asset Viewer ===" << std::endl;

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Request OpenGL context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    app.window = SDL_CreateWindow(WIN_TITLE, WIN_W, WIN_H,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!app.window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    app.gl_ctx = SDL_GL_CreateContext(app.window);
    if (!app.gl_ctx) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1); // VSync

    // Set window icon (same as launcher)
    {
        const char* icon_paths[] = {
            "src/assets/launcer_icon.png",
            "src/assets/icon_gnome.png",
            "/usr/share/icons/hicolor/128x128/apps/swordigo-desktop.png",
            "/usr/share/pixmaps/swordigo-desktop.png",
            nullptr
        };
        SDL_Surface* icon = nullptr;
        for (int i = 0; icon_paths[i]; i++) {
            icon = IMG_Load(icon_paths[i]);
            if (icon) break;
        }
        if (icon) {
            SDL_SetWindowIcon(app.window, icon);
            SDL_DestroySurface(icon);
        }
    }

    std::cout << "[AV] OpenGL: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "[AV] Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Find and load starting directory
    std::string start_dir = find_start_dir(argc > 1 ? argv[1] : nullptr);
    std::cout << "[AV] Starting in: " << start_dir << std::endl;
    load_directory(start_dir);

    // Main loop
    while (app.running) {
        handle_events();
        render_frame();
        SDL_GL_SwapWindow(app.window);
        SDL_Delay(16); // ~60fps cap
    }

    // Cleanup
    clear_preview();
    SDL_GL_DestroyContext(app.gl_ctx);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    std::cout << "[AV] Goodbye!" << std::endl;
    return 0;
}
