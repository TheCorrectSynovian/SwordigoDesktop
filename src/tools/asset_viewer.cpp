/* asset_viewer.cpp — Professional ImGui-based asset viewer for Swordigo Desktop
 *
 * Features:
 *   - File browser with search, type filtering, icons
 *   - 3D POD model viewport with orbit camera, wireframe, texturing
 *   - PVR/PNG texture preview with zoom/pan and checkerboard background
 *   - Audio WAV playback with waveform visualization
 *   - Scene file inspection with object tree and component details
 *   - Properties panel with per-type metadata
 *   - Keyboard shortcuts: W(wireframe), T(textured), R(reset camera), Esc(quit)
 *
 * Build:  make asset_viewer
 * Usage:  ./asset_viewer [optional_start_directory]
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

#include "platform/pvr_loader.h"
#include "tools/pod_loader.h"
#include "tools/av_renderer.h"
#include "tools/av_audio.h"
#include "tools/scene_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// Constants
// ============================================================================

static const int   WIN_W             = 1400;
static const int   WIN_H             = 900;
static const char* WIN_TITLE         = "Swordigo Asset Viewer";
static const float LEFT_PANEL_W      = 280.0f;
static const float RIGHT_PANEL_W     = 300.0f;
static const float STATUS_BAR_H      = 24.0f;
static const char* GLSL_VERSION      = "#version 330";

// ============================================================================
// File entry & type classification
// ============================================================================

enum FileType { FTYPE_OTHER = 0, FTYPE_TEXTURE = 1, FTYPE_MODEL = 2, FTYPE_SCENE = 3, FTYPE_AUDIO = 4 };
enum PreviewType { PREVIEW_NONE = 0, PREVIEW_TEXTURE, PREVIEW_MODEL, PREVIEW_SCENE, PREVIEW_AUDIO };

struct FileEntry {
    std::string name;
    std::string full_path;
    bool        is_dir;
    size_t      size;
    int         type; // FileType
};

static int classify_file(const std::string& name) {
    // Get extension, lowercase
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return FTYPE_OTHER;
    std::string ext = name.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);

    if (ext == ".pvr" || ext == ".png" || ext == ".jpg" || ext == ".jpeg") return FTYPE_TEXTURE;
    // Also handle .tex.png pattern
    if (name.size() > 8) {
        std::string low = name;
        for (auto& c : low) c = (char)tolower((unsigned char)c);
        if (low.find(".tex.png") != std::string::npos) return FTYPE_TEXTURE;
    }
    if (ext == ".pod") return FTYPE_MODEL;
    if (ext == ".scene") return FTYPE_SCENE;
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return FTYPE_AUDIO;
    return FTYPE_OTHER;
}

// ============================================================================
// Viewer state
// ============================================================================

struct ViewerState {
    // File browser
    std::string            current_dir;
    std::vector<FileEntry> files;
    std::vector<FileEntry> filtered_files; // after search + type filter
    int                    selected_idx = -1;
    char                   search_buf[256] = {};
    int                    type_filter = 0; // 0=all, 1-4=specific

    // Preview
    PreviewType preview_type = PREVIEW_NONE;
    std::string status_msg;

    // Texture preview
    GLuint preview_tex = 0;
    int    tex_w = 0, tex_h = 0;
    float  tex_zoom = 1.0f;
    float  tex_pan_x = 0.0f, tex_pan_y = 0.0f;
    bool   tex_dragging = false;
    std::string tex_format_str;

    // Model preview
    av::PODModel             model;
    std::vector<av::GPUMesh> gpu_meshes;
    av::Camera               camera;
    GLuint                   fbo = 0, fbo_tex = 0;
    int                      fbo_w = 0, fbo_h = 0;
    bool                     show_wireframe = false;
    bool                     show_textured  = true;
    GLuint                   model_texture  = 0;
    int                      highlighted_mesh = -1;

    // Scene preview
    av::SceneData scene;
    int           selected_object = -1;

    // Checkerboard texture (for transparency)
    GLuint checker_tex = 0;

    // Selected file info
    std::string sel_name;
    std::string sel_path;
    size_t      sel_size = 0;
};

static ViewerState g_state;

// ============================================================================
// Helpers
// ============================================================================

static std::string expand_home(const std::string& p) {
    if (!p.empty() && p[0] == '~') {
        const char* home = getenv("HOME");
        if (home) return std::string(home) + p.substr(1);
    }
    return p;
}

static std::string format_size(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[64]; snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[64]; snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

static std::string format_time(float seconds) {
    int m = (int)(seconds / 60.0f);
    int s = (int)seconds % 60;
    char buf[32]; snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

static const char* filetype_label(int ft) {
    switch (ft) {
        case FTYPE_TEXTURE: return "Texture";
        case FTYPE_MODEL:   return "POD Model";
        case FTYPE_SCENE:   return "Scene";
        case FTYPE_AUDIO:   return "Audio";
        default:            return "File";
    }
}

// ============================================================================
// Create checkerboard texture for alpha backgrounds
// ============================================================================

static GLuint create_checkerboard() {
    const int sz = 64;
    const int cell = 8;
    unsigned char pixels[sz * sz * 4];
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            bool light = ((x / cell) + (y / cell)) % 2 == 0;
            unsigned char v = light ? 200 : 150;
            int i = (y * sz + x) * 4;
            pixels[i] = pixels[i+1] = pixels[i+2] = v;
            pixels[i+3] = 255;
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

// ============================================================================
// Directory listing
// ============================================================================

static void refresh_directory(ViewerState& st) {
    st.files.clear();
    st.filtered_files.clear();
    st.selected_idx = -1;

    std::error_code ec;
    if (!fs::is_directory(st.current_dir, ec)) return;

    // ".." entry
    fs::path parent = fs::path(st.current_dir).parent_path();
    if (!parent.empty() && parent != st.current_dir) {
        FileEntry up;
        up.name = "..";
        up.full_path = parent.string();
        up.is_dir = true;
        up.size = 0;
        up.type = FTYPE_OTHER;
        st.files.push_back(up);
    }

    for (auto& entry : fs::directory_iterator(st.current_dir, ec)) {
        FileEntry fe;
        fe.name = entry.path().filename().string();
        fe.full_path = entry.path().string();
        fe.is_dir = entry.is_directory(ec);
        fe.size = fe.is_dir ? 0 : (size_t)entry.file_size(ec);
        fe.type = fe.is_dir ? FTYPE_OTHER : classify_file(fe.name);
        st.files.push_back(fe);
    }

    // Sort: directories first (except ".."), then alphabetical case-insensitive
    std::sort(st.files.begin(), st.files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.name == "..") return true;
        if (b.name == "..") return false;
        if (a.is_dir != b.is_dir) return a.is_dir;
        // Case-insensitive compare
        std::string la = a.name, lb = b.name;
        for (auto& c : la) c = (char)tolower((unsigned char)c);
        for (auto& c : lb) c = (char)tolower((unsigned char)c);
        return la < lb;
    });
}

static void apply_filters(ViewerState& st) {
    st.filtered_files.clear();
    std::string search_lower(st.search_buf);
    for (auto& c : search_lower) c = (char)tolower((unsigned char)c);

    for (auto& f : st.files) {
        // Type filter
        if (st.type_filter != 0 && !f.is_dir && f.type != st.type_filter) continue;

        // Search filter
        if (!search_lower.empty()) {
            std::string name_lower = f.name;
            for (auto& c : name_lower) c = (char)tolower((unsigned char)c);
            if (name_lower.find(search_lower) == std::string::npos) continue;
        }
        st.filtered_files.push_back(f);
    }
}

// ============================================================================
// Resource cleanup
// ============================================================================

static void free_preview_resources(ViewerState& st) {
    // Free GPU meshes
    for (auto& m : st.gpu_meshes) av::free_mesh(m);
    st.gpu_meshes.clear();
    st.model = av::PODModel{};

    // Free preview texture
    if (st.preview_tex) { glDeleteTextures(1, &st.preview_tex); st.preview_tex = 0; }
    st.tex_w = st.tex_h = 0;
    st.tex_zoom = 1.0f;
    st.tex_pan_x = st.tex_pan_y = 0.0f;
    st.tex_format_str.clear();

    // Free model texture
    if (st.model_texture) { glDeleteTextures(1, &st.model_texture); st.model_texture = 0; }

    // Stop audio
    av::audio_stop();

    // Clear scene
    st.scene = av::SceneData{};
    st.selected_object = -1;
    st.highlighted_mesh = -1;

    st.preview_type = PREVIEW_NONE;
    st.status_msg.clear();
}

// ============================================================================
// File selection / loading
// ============================================================================

static void try_load_matching_texture(ViewerState& st, const std::string& pod_path) {
    // Given "somedir/model.POD", try "somedir/model_2x.pvr"
    fs::path p(pod_path);
    std::string stem = p.stem().string();
    fs::path dir = p.parent_path();

    // Try _2x.pvr first, then .pvr, then _2x.png, then .png
    const char* suffixes[] = {"_2x.pvr", ".pvr", "_2x.png", ".png"};
    for (auto& suf : suffixes) {
        fs::path candidate = dir / (stem + suf);
        if (fs::exists(candidate)) {
            std::string cpath = candidate.string();
            std::string ext = candidate.extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);

            if (ext == ".pvr") {
                st.model_texture = pvr_load_texture(cpath.c_str(), nullptr, nullptr);
            } else {
                // Use SDL_image for PNG
                SDL_Surface* surf = IMG_Load(cpath.c_str());
                if (surf) {
                    // Convert to RGBA32
                    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
                    SDL_DestroySurface(surf);
                    if (conv) {
                        GLuint tex;
                        glGenTextures(1, &tex);
                        glBindTexture(GL_TEXTURE_2D, tex);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0,
                                     GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        st.model_texture = tex;
                        SDL_DestroySurface(conv);
                    }
                }
            }
            if (st.model_texture) return;
        }
    }
}

static void select_file(ViewerState& st, const FileEntry& fe) {
    if (fe.is_dir) {
        st.current_dir = fe.full_path;
        refresh_directory(st);
        apply_filters(st);
        return;
    }

    free_preview_resources(st);
    st.sel_name = fe.name;
    st.sel_path = fe.full_path;
    st.sel_size = fe.size;

    switch (fe.type) {
    case FTYPE_TEXTURE: {
        st.preview_type = PREVIEW_TEXTURE;
        std::string ext = fs::path(fe.name).extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);

        if (ext == ".pvr") {
            st.preview_tex = pvr_load_texture(fe.full_path.c_str(), &st.tex_w, &st.tex_h);
            st.tex_format_str = "ETC1 (PVR)";
        } else {
            SDL_Surface* surf = IMG_Load(fe.full_path.c_str());
            if (surf) {
                SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(surf);
                if (conv) {
                    GLuint tex;
                    glGenTextures(1, &tex);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    st.preview_tex = tex;
                    st.tex_w = conv->w;
                    st.tex_h = conv->h;
                    st.tex_format_str = "RGBA8 (" + std::string(ext.c_str() + 1) + ")";
                    SDL_DestroySurface(conv);
                }
            }
        }
        if (st.preview_tex)
            st.status_msg = "Loaded " + fe.name + " — " + std::to_string(st.tex_w) + "x" + std::to_string(st.tex_h);
        else
            st.status_msg = "Failed to load " + fe.name;
    } break;

    case FTYPE_MODEL: {
        st.preview_type = PREVIEW_MODEL;
        st.model = av::pod_load(fe.full_path);

        if (st.model.meshes.empty()) {
            st.status_msg = "Failed to load " + fe.name;
            st.preview_type = PREVIEW_NONE;
            break;
        }

        // Upload each mesh to the GPU
        for (auto& mesh : st.model.meshes) {
            const float*    pos = mesh.positions.empty()  ? nullptr : mesh.positions.data();
            const float*    nrm = mesh.normals.empty()    ? nullptr : mesh.normals.data();
            const float*    uv  = mesh.uvs.empty()        ? nullptr : mesh.uvs.data();
            const uint16_t* idx = mesh.indices.empty()    ? nullptr : mesh.indices.data();
            av::GPUMesh gm = av::upload_mesh(pos, nrm, uv, mesh.num_vertices,
                                              idx, (int)mesh.indices.size());
            st.gpu_meshes.push_back(gm);
        }

        // Set camera to frame the model
        st.camera = av::Camera{};
        st.camera.target[0] = st.model.center_x;
        st.camera.target[1] = st.model.center_y;
        st.camera.target[2] = st.model.center_z;
        st.camera.distance  = st.model.radius * 2.5f;
        if (st.camera.distance < 1.0f) st.camera.distance = 3.0f;

        // Try auto-loading a matching texture
        try_load_matching_texture(st, fe.full_path);

        // Bind texture to each GPU mesh
        if (st.model_texture) {
            for (auto& gm : st.gpu_meshes) gm.texture_id = st.model_texture;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "Loaded %s — %d mesh(es), %d verts, %d faces",
                 fe.name.c_str(), (int)st.model.meshes.size(),
                 st.model.total_vertices, st.model.total_faces);
        st.status_msg = buf;
    } break;

    case FTYPE_SCENE: {
        st.preview_type = PREVIEW_SCENE;
        st.scene = av::scene_load(fe.full_path);
        if (st.scene.objects.empty()) {
            st.status_msg = "Failed to parse " + fe.name;
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "Loaded %s — %d objects",
                     fe.name.c_str(), (int)st.scene.objects.size());
            st.status_msg = buf;
        }
    } break;

    case FTYPE_AUDIO: {
        st.preview_type = PREVIEW_AUDIO;
        if (av::audio_load(fe.full_path)) {
            auto& as = av::audio_get_state();
            char buf[256];
            snprintf(buf, sizeof(buf), "Loaded %s — %s, %d Hz, %d-bit",
                     fe.name.c_str(), format_time(as.duration).c_str(),
                     as.sample_rate, as.bits_per_sample);
            st.status_msg = buf;
        } else {
            st.status_msg = "Failed to load " + fe.name;
            st.preview_type = PREVIEW_NONE;
        }
    } break;

    default:
        st.preview_type = PREVIEW_NONE;
        st.status_msg = fe.name + " — no preview available";
        break;
    }
}

// ============================================================================
// UI: File browser panel (left)
// ============================================================================

static void draw_file_browser(ViewerState& st) {
    ImGui::BeginChild("FileBrowser", ImVec2(LEFT_PANEL_W, 0), ImGuiChildFlags_Borders);

    // Search bar
    ImGui::PushItemWidth(-1);
    bool search_changed = ImGui::InputTextWithHint("##search", "Search files...",
                                                    st.search_buf, sizeof(st.search_buf));
    ImGui::PopItemWidth();

    // Filter buttons
    ImGui::Spacing();
    const char* labels[] = {"All", "Tex", "Model", "Scene", "Audio"};
    for (int i = 0; i < 5; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (st.type_filter == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.70f, 1.0f));
        if (ImGui::SmallButton(labels[i])) {
            st.type_filter = i;
            search_changed = true;
        }
        if (active) ImGui::PopStyleColor();
    }
    ImGui::Separator();

    if (search_changed) apply_filters(st);

    // File list
    ImGui::BeginChild("FileList", ImVec2(0, 0));
    for (int i = 0; i < (int)st.filtered_files.size(); i++) {
        auto& f = st.filtered_files[i];

        // Icon/color based on type
        ImVec4 color(0.90f, 0.90f, 0.94f, 1.0f); // default text
        const char* icon = "   ";
        if (f.is_dir)                { icon = "\xF0\x9F\x93\x81 "; color = ImVec4(1.0f, 0.85f, 0.35f, 1.0f); } // 📁
        else if (f.type == FTYPE_TEXTURE) { icon = "\xF0\x9F\x8E\xA8 "; color = ImVec4(0.45f, 0.85f, 0.45f, 1.0f); } // 🎨
        else if (f.type == FTYPE_MODEL)   { icon = "\xF0\x9F\x94\xB7 "; color = ImVec4(0.45f, 0.65f, 1.0f, 1.0f); }  // 🔷
        else if (f.type == FTYPE_SCENE)   { icon = "\xF0\x9F\x8F\x97 "; color = ImVec4(0.75f, 0.55f, 1.0f, 1.0f); }  // 🏗
        else if (f.type == FTYPE_AUDIO)   { icon = "\xF0\x9F\x8E\xB5 "; color = ImVec4(1.0f, 0.65f, 0.30f, 1.0f); }  // 🎵

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        char label[512];
        snprintf(label, sizeof(label), "%s%s##%d", icon, f.name.c_str(), i);
        bool selected = (i == st.selected_idx);
        if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            st.selected_idx = i;
            if (ImGui::IsMouseDoubleClicked(0) || f.is_dir) {
                select_file(st, f);
            } else {
                // Single click: load file preview
                select_file(st, f);
            }
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

// ============================================================================
// UI: Center panel — 3D model viewport
// ============================================================================

static void draw_model_viewport(ViewerState& st) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (int)avail.x;
    int h = (int)avail.y;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    // Create or resize FBO
    if (!st.fbo) {
        st.fbo = av::create_fbo(w, h, &st.fbo_tex);
        st.fbo_w = w; st.fbo_h = h;
    } else if (w != st.fbo_w || h != st.fbo_h) {
        av::resize_fbo(st.fbo, w, h, &st.fbo_tex);
        st.fbo_w = w; st.fbo_h = h;
    }

    // Render the scene into FBO
    av::begin_3d(st.fbo, w, h, st.camera);
    av::render_grid(20.0f, st.model.min_y);

    float identity[16];
    av::mat4_identity(identity);
    float white[4] = {1, 1, 1, 1};
    float highlight[4] = {0.4f, 0.7f, 1.0f, 1.0f};

    // Bind model texture if textured mode
    for (int i = 0; i < (int)st.gpu_meshes.size(); i++) {
        auto& gm = st.gpu_meshes[i];
        if (st.show_textured && st.model_texture) {
            gm.texture_id = st.model_texture;
        } else {
            gm.texture_id = 0;
        }

        float* col = (i == st.highlighted_mesh) ? highlight : white;
        av::render_mesh(gm, identity, col, false);

        if (st.show_wireframe) {
            float wire_col[4] = {0.2f, 0.8f, 1.0f, 0.5f};
            av::render_mesh(gm, identity, wire_col, true);
        }
    }
    av::end_3d();

    // Display FBO as ImGui image
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)st.fbo_tex, ImVec2((float)w, (float)h),
                 ImVec2(0, 1), ImVec2(1, 0)); // flip Y for OpenGL

    // Mouse interaction on viewport
    if (ImGui::IsItemHovered()) {
        ImGuiIO& io = ImGui::GetIO();

        // Scroll to zoom
        if (io.MouseWheel != 0.0f) {
            st.camera.distance *= (1.0f - io.MouseWheel * 0.1f);
            if (st.camera.distance < 0.1f) st.camera.distance = 0.1f;
            if (st.camera.distance > 500.0f) st.camera.distance = 500.0f;
        }

        // Left drag: orbit
        if (ImGui::IsMouseDragging(0)) {
            ImVec2 delta = io.MouseDelta;
            st.camera.yaw   += delta.x * 0.5f;
            st.camera.pitch += delta.y * 0.5f;
            if (st.camera.pitch >  89.0f) st.camera.pitch =  89.0f;
            if (st.camera.pitch < -89.0f) st.camera.pitch = -89.0f;
        }

        // Right drag: pan
        if (ImGui::IsMouseDragging(1)) {
            ImVec2 delta = io.MouseDelta;
            float scale = st.camera.distance * 0.003f;
            // Pan in camera-local XY plane
            float yaw_rad = st.camera.yaw * 3.14159f / 180.0f;
            st.camera.target[0] -= (cosf(yaw_rad) * delta.x + sinf(yaw_rad) * 0) * scale;
            st.camera.target[2] -= (-sinf(yaw_rad) * delta.x + cosf(yaw_rad) * 0) * scale;
            st.camera.target[1] += delta.y * scale;
        }
    }
}

// ============================================================================
// UI: Center panel — Texture preview
// ============================================================================

static void draw_texture_preview(ViewerState& st) {
    if (!st.preview_tex) {
        ImGui::TextDisabled("Failed to load texture.");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 img_size((float)st.tex_w * st.tex_zoom, (float)st.tex_h * st.tex_zoom);

    // Center the image
    float off_x = (avail.x - img_size.x) * 0.5f + st.tex_pan_x;
    float off_y = (avail.y - img_size.y) * 0.5f + st.tex_pan_y;

    // Draw checkerboard background behind the image area
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Checkerboard behind texture
    ImVec2 img_min(cursor.x + off_x, cursor.y + off_y);
    ImVec2 img_max(img_min.x + img_size.x, img_min.y + img_size.y);
    if (st.checker_tex) {
        float uv_scale_x = img_size.x / 32.0f;
        float uv_scale_y = img_size.y / 32.0f;
        dl->AddImage((ImTextureID)(intptr_t)st.checker_tex,
                     img_min, img_max, ImVec2(0, 0), ImVec2(uv_scale_x, uv_scale_y));
    }

    // Draw the actual texture
    dl->AddImage((ImTextureID)(intptr_t)st.preview_tex, img_min, img_max);

    // Invisible button to capture mouse events
    ImGui::InvisibleButton("##tex_area", avail);
    if (ImGui::IsItemHovered()) {
        ImGuiIO& io = ImGui::GetIO();
        // Scroll to zoom
        if (io.MouseWheel != 0.0f) {
            float old_zoom = st.tex_zoom;
            st.tex_zoom *= (1.0f + io.MouseWheel * 0.1f);
            if (st.tex_zoom < 0.1f) st.tex_zoom = 0.1f;
            if (st.tex_zoom > 32.0f) st.tex_zoom = 32.0f;

            // Zoom towards mouse position
            float mx = io.MousePos.x - cursor.x - avail.x * 0.5f;
            float my = io.MousePos.y - cursor.y - avail.y * 0.5f;
            float factor = st.tex_zoom / old_zoom;
            st.tex_pan_x = mx - factor * (mx - st.tex_pan_x);
            st.tex_pan_y = my - factor * (my - st.tex_pan_y);
        }
        // Drag to pan
        if (ImGui::IsMouseDragging(0)) {
            st.tex_pan_x += io.MouseDelta.x;
            st.tex_pan_y += io.MouseDelta.y;
        }
    }
}

// ============================================================================
// UI: Center panel — Scene inspector
// ============================================================================

static void draw_scene_inspector(ViewerState& st) {
    if (st.scene.objects.empty()) {
        ImGui::TextDisabled("No objects in scene.");
        return;
    }

    ImGui::Text("Scene: %s — %d objects", st.scene.filename.c_str(), (int)st.scene.objects.size());
    ImGui::Separator();

    ImGui::BeginChild("SceneTree", ImVec2(0, 0));
    for (int i = 0; i < (int)st.scene.objects.size(); i++) {
        auto& obj = st.scene.objects[i];
        bool is_selected = (i == st.selected_object);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
        if (obj.components.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", obj.name.c_str());
        if (ImGui::IsItemClicked()) st.selected_object = i;

        if (open) {
            // Transform
            ImGui::TextDisabled("Position: (%.2f, %.2f, %.2f)", obj.pos_x, obj.pos_y, obj.pos_z);
            ImGui::TextDisabled("Rotation: (%.2f, %.2f, %.2f)", obj.rot_x, obj.rot_y, obj.rot_z);
            ImGui::TextDisabled("Scale:    (%.2f, %.2f, %.2f)", obj.scale_x, obj.scale_y, obj.scale_z);

            // Components
            for (auto& comp : obj.components) {
                ImGui::BulletText("%s (id=%d)", comp.type_name.c_str(), comp.type_id);
            }

            // Mesh/texture references
            if (!obj.mesh_name.empty())
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Mesh: %s", obj.mesh_name.c_str());
            if (!obj.texture_name.empty())
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Texture: %s", obj.texture_name.c_str());

            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
}

// ============================================================================
// UI: Center panel — Audio player
// ============================================================================

static void draw_audio_player(ViewerState& st) {
    auto& as = av::audio_get_state();
    if (!as.loaded) {
        ImGui::TextDisabled("No audio loaded.");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Waveform display — takes most of the space
    float waveform_h = avail.y - 90.0f;
    if (waveform_h < 50.0f) waveform_h = 50.0f;

    if (!as.waveform.empty()) {
        // Draw waveform with playback position indicator
        ImVec2 wf_pos = ImGui::GetCursorScreenPos();

        ImGui::PlotLines("##waveform", as.waveform.data(), (int)as.waveform.size(),
                         0, nullptr, -1.0f, 1.0f, ImVec2(avail.x, waveform_h));

        // Playback position line
        if (as.duration > 0.0f) {
            float progress = as.position / as.duration;
            float line_x = wf_pos.x + progress * avail.x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddLine(ImVec2(line_x, wf_pos.y),
                        ImVec2(line_x, wf_pos.y + waveform_h),
                        IM_COL32(255, 100, 50, 255), 2.0f);
        }
    } else {
        ImGui::TextDisabled("No waveform data available.");
        ImGui::Dummy(ImVec2(0, waveform_h));
    }

    ImGui::Spacing();

    // Controls row
    float button_w = 32.0f;

    // Play/Pause
    if (as.playing && !as.paused) {
        if (ImGui::Button("\xE2\x8F\xB8", ImVec2(button_w, 0)))  // ⏸
            av::audio_pause();
    } else {
        if (ImGui::Button("\xE2\x96\xB6", ImVec2(button_w, 0)))  // ▶
            av::audio_play();
    }
    ImGui::SameLine();

    // Stop
    if (ImGui::Button("\xE2\x8F\xB9", ImVec2(button_w, 0)))  // ⏹
        av::audio_stop();
    ImGui::SameLine();

    // Time display
    ImGui::Text("%s / %s", format_time(as.position).c_str(), format_time(as.duration).c_str());
    ImGui::SameLine();

    // Seek slider
    float seek_w = avail.x - 350.0f;
    if (seek_w < 100.0f) seek_w = 100.0f;
    ImGui::PushItemWidth(seek_w);
    float pos = as.position;
    if (ImGui::SliderFloat("##seek", &pos, 0.0f, as.duration, "%.1fs")) {
        av::audio_seek(pos);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Volume slider
    ImGui::PushItemWidth(100.0f);
    float vol = as.volume;
    if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.0f, "Vol:%.0f%%")) {
        av::audio_set_volume(vol);
    }
    ImGui::PopItemWidth();
}

// ============================================================================
// UI: Center panel dispatcher
// ============================================================================

static void draw_center_panel(ViewerState& st) {
    switch (st.preview_type) {
        case PREVIEW_MODEL:   draw_model_viewport(st);  break;
        case PREVIEW_TEXTURE: draw_texture_preview(st);  break;
        case PREVIEW_SCENE:   draw_scene_inspector(st);  break;
        case PREVIEW_AUDIO:   draw_audio_player(st);     break;
        default: {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 text_size = ImGui::CalcTextSize("No file selected");
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetCursorPosX() + (avail.x - text_size.x) * 0.5f,
                ImGui::GetCursorPosY() + (avail.y - text_size.y) * 0.5f));
            ImGui::TextDisabled("No file selected");
        } break;
    }
}

// ============================================================================
// UI: Properties panel (right)
// ============================================================================

static void draw_properties_panel(ViewerState& st) {
    ImGui::BeginChild("Properties", ImVec2(RIGHT_PANEL_W, 0), ImGuiChildFlags_Borders);

    ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "Properties");
    ImGui::Separator();

    if (st.preview_type == PREVIEW_NONE && st.sel_name.empty()) {
        ImGui::TextDisabled("Select a file to view properties.");
        ImGui::EndChild();
        return;
    }

    // Common info
    ImGui::Text("Name: %s", st.sel_name.c_str());
    ImGui::Text("Size: %s", format_size(st.sel_size).c_str());
    ImGui::Text("Type: %s", filetype_label(
        st.preview_type == PREVIEW_TEXTURE ? FTYPE_TEXTURE :
        st.preview_type == PREVIEW_MODEL   ? FTYPE_MODEL   :
        st.preview_type == PREVIEW_SCENE   ? FTYPE_SCENE   :
        st.preview_type == PREVIEW_AUDIO   ? FTYPE_AUDIO   : FTYPE_OTHER));
    ImGui::TextWrapped("Path: %s", st.sel_path.c_str());
    ImGui::Separator();

    switch (st.preview_type) {
    case PREVIEW_MODEL: {
        ImGui::Text("Version: %s", st.model.version.c_str());
        ImGui::Text("Meshes:   %d", (int)st.model.meshes.size());
        ImGui::Text("Vertices: %d", st.model.total_vertices);
        ImGui::Text("Faces:    %d", st.model.total_faces);
        ImGui::Text("Frames:   %d", st.model.num_frames);
        ImGui::Separator();

        // Bounding box
        if (ImGui::TreeNode("Bounding Box")) {
            ImGui::Text("Min: (%.3f, %.3f, %.3f)", st.model.min_x, st.model.min_y, st.model.min_z);
            ImGui::Text("Max: (%.3f, %.3f, %.3f)", st.model.max_x, st.model.max_y, st.model.max_z);
            ImGui::Text("Center: (%.3f, %.3f, %.3f)", st.model.center_x, st.model.center_y, st.model.center_z);
            ImGui::Text("Radius: %.3f", st.model.radius);
            ImGui::TreePop();
        }
        ImGui::Separator();

        // Mesh list
        if (ImGui::TreeNode("Meshes")) {
            for (int i = 0; i < (int)st.model.meshes.size(); i++) {
                auto& m = st.model.meshes[i];
                char lbl[64];
                snprintf(lbl, sizeof(lbl), "Mesh %d (%d verts)##m%d", i, m.num_vertices, i);
                if (ImGui::Selectable(lbl, i == st.highlighted_mesh)) {
                    st.highlighted_mesh = (st.highlighted_mesh == i) ? -1 : i;
                }
            }
            ImGui::TreePop();
        }
        ImGui::Separator();

        // Render controls
        ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "Render Controls");
        ImGui::Checkbox("Textured", &st.show_textured);
        ImGui::Checkbox("Wireframe", &st.show_wireframe);
        ImGui::Text("Texture: %s", st.model_texture ? "Loaded" : "None");
    } break;

    case PREVIEW_TEXTURE: {
        ImGui::Text("Width:  %d px", st.tex_w);
        ImGui::Text("Height: %d px", st.tex_h);
        ImGui::Text("Format: %s", st.tex_format_str.c_str());
        ImGui::Text("Zoom:   %.0f%%", st.tex_zoom * 100.0f);
        ImGui::Separator();
        if (ImGui::Button("Reset Zoom")) {
            st.tex_zoom = 1.0f;
            st.tex_pan_x = st.tex_pan_y = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Fit")) {
            // Auto-fit zoom will be computed next frame based on avail size
            st.tex_zoom = 1.0f;
            st.tex_pan_x = st.tex_pan_y = 0.0f;
        }
    } break;

    case PREVIEW_SCENE: {
        ImGui::Text("Objects: %d", (int)st.scene.objects.size());
        ImGui::Separator();
        if (st.selected_object >= 0 && st.selected_object < (int)st.scene.objects.size()) {
            auto& obj = st.scene.objects[st.selected_object];
            ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "Selected Object");
            ImGui::Text("Name: %s", obj.name.c_str());
            ImGui::Text("Components: %d", (int)obj.components.size());
            ImGui::Separator();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", obj.pos_x, obj.pos_y, obj.pos_z);
            ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", obj.rot_x, obj.rot_y, obj.rot_z);
            ImGui::Text("Scale:    (%.2f, %.2f, %.2f)", obj.scale_x, obj.scale_y, obj.scale_z);
            if (!obj.mesh_name.empty())
                ImGui::Text("Mesh: %s", obj.mesh_name.c_str());
            if (!obj.texture_name.empty())
                ImGui::Text("Texture: %s", obj.texture_name.c_str());
        }
    } break;

    case PREVIEW_AUDIO: {
        auto& as = av::audio_get_state();
        ImGui::Text("Sample Rate: %d Hz", as.sample_rate);
        ImGui::Text("Channels:    %d", as.channels);
        ImGui::Text("Bit Depth:   %d", as.bits_per_sample);
        ImGui::Text("Duration:    %s", format_time(as.duration).c_str());
        ImGui::Text("Status:      %s",
                     as.playing ? (as.paused ? "Paused" : "Playing") : "Stopped");
    } break;

    default:
        break;
    }

    ImGui::EndChild();
}

// ============================================================================
// UI: Status bar (bottom)
// ============================================================================

static void draw_status_bar(ViewerState& st) {
    ImGui::Separator();
    ImGui::BeginChild("StatusBar", ImVec2(0, STATUS_BAR_H));

    // Left: directory
    ImGui::TextDisabled("%s", st.current_dir.c_str());

    // Center: status
    if (!st.status_msg.empty()) {
        float text_w = ImGui::CalcTextSize(st.status_msg.c_str()).x;
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(avail * 0.3f);
        ImGui::Text("%s", st.status_msg.c_str());
    }

    // Right: shortcuts
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200.0f + ImGui::GetCursorPosX());
    ImGui::TextDisabled("[W]ire [T]ex [R]eset [Esc]Quit");

    ImGui::EndChild();
}

// ============================================================================
// Apply ImGui theme
// ============================================================================

static void apply_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 0;
    style.FrameRounding    = 4;
    style.ScrollbarRounding = 6;
    style.GrabRounding     = 3;
    style.WindowBorderSize = 0;
    style.FrameBorderSize  = 0;
    style.ItemSpacing      = ImVec2(8, 6);
    style.FramePadding     = ImVec2(8, 4);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.14f, 0.14f, 0.19f, 0.95f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.16f, 0.16f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.28f, 0.28f, 0.38f, 1.0f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.08f, 0.08f, 0.11f, 1.0f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.12f, 0.12f, 0.17f, 1.0f);
    c[ImGuiCol_Header]          = ImVec4(0.20f, 0.22f, 0.32f, 1.0f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.28f, 0.30f, 0.42f, 1.0f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.34f, 0.36f, 0.48f, 1.0f);
    c[ImGuiCol_Button]          = ImVec4(0.24f, 0.28f, 0.40f, 1.0f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.32f, 0.38f, 0.55f, 1.0f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.40f, 0.45f, 0.62f, 1.0f);
    c[ImGuiCol_Tab]             = ImVec4(0.16f, 0.16f, 0.22f, 1.0f);
    c[ImGuiCol_TabHovered]      = ImVec4(0.28f, 0.30f, 0.42f, 1.0f);
    c[ImGuiCol_TabActive]       = ImVec4(0.22f, 0.24f, 0.35f, 1.0f);
    c[ImGuiCol_Separator]       = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.30f, 0.30f, 0.40f, 1.0f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.45f, 0.65f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.45f, 0.55f, 0.80f, 1.0f);
    c[ImGuiCol_Text]            = ImVec4(0.90f, 0.90f, 0.94f, 1.0f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.50f, 0.50f, 0.58f, 1.0f);
}

// ============================================================================
// Keyboard shortcut handling (called when ImGui doesn't want keyboard)
// ============================================================================

static bool handle_shortcuts(ViewerState& st) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return false;

    // Escape: quit
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) return true;

    // W: toggle wireframe
    if (ImGui::IsKeyPressed(ImGuiKey_W)) st.show_wireframe = !st.show_wireframe;

    // T: toggle textured
    if (ImGui::IsKeyPressed(ImGuiKey_T)) st.show_textured = !st.show_textured;

    // R: reset camera
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        st.camera = av::Camera{};
        if (st.preview_type == PREVIEW_MODEL) {
            st.camera.target[0] = st.model.center_x;
            st.camera.target[1] = st.model.center_y;
            st.camera.target[2] = st.model.center_z;
            st.camera.distance  = st.model.radius * 2.5f;
            if (st.camera.distance < 1.0f) st.camera.distance = 3.0f;
        }
    }

    // F1-F5: filter types
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) { st.type_filter = 0; apply_filters(st); }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) { st.type_filter = 1; apply_filters(st); }
    if (ImGui::IsKeyPressed(ImGuiKey_F3)) { st.type_filter = 2; apply_filters(st); }
    if (ImGui::IsKeyPressed(ImGuiKey_F4)) { st.type_filter = 3; apply_filters(st); }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) { st.type_filter = 4; apply_filters(st); }

    // Up/Down: navigate file list
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        if (st.selected_idx > 0) {
            st.selected_idx--;
            select_file(st, st.filtered_files[st.selected_idx]);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        if (st.selected_idx < (int)st.filtered_files.size() - 1) {
            st.selected_idx++;
            select_file(st, st.filtered_files[st.selected_idx]);
        }
    }

    // Enter: open directory
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (st.selected_idx >= 0 && st.selected_idx < (int)st.filtered_files.size()) {
            auto& f = st.filtered_files[st.selected_idx];
            if (f.is_dir) select_file(st, f);
        }
    }

    // Backspace: go up
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        fs::path parent = fs::path(st.current_dir).parent_path();
        if (!parent.empty() && parent != st.current_dir) {
            st.current_dir = parent.string();
            refresh_directory(st);
            apply_filters(st);
        }
    }

    return false;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    // ── SDL init ────────────────────────────────────────────────────────
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // OpenGL 3.3 core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(WIN_TITLE, WIN_W, WIN_H,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); // VSync

    // ── ImGui init ──────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    apply_theme();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    // ── Subsystem init ──────────────────────────────────────────────────
    if (!av::renderer_init()) {
        fprintf(stderr, "Failed to initialize renderer.\n");
    }
    if (!av::audio_init()) {
        fprintf(stderr, "Failed to initialize audio.\n");
    }

    // Checkerboard texture for alpha backgrounds
    g_state.checker_tex = create_checkerboard();

    // ── Default directory ───────────────────────────────────────────────
    if (argc > 1) {
        g_state.current_dir = expand_home(argv[1]);
    } else {
        g_state.current_dir = expand_home("~/.local/share/swordigo-desktop/assets/resources/");
    }
    // Fallback: if the directory doesn't exist, use home
    if (!fs::is_directory(g_state.current_dir)) {
        const char* home = getenv("HOME");
        g_state.current_dir = home ? home : "/";
    }

    refresh_directory(g_state);
    apply_filters(g_state);
    g_state.status_msg = "Ready — browse files on the left";

    // ── Main loop ───────────────────────────────────────────────────────
    bool running = true;
    while (running) {
        // Poll events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }

        // Update audio
        av::audio_update();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Keyboard shortcuts
        if (handle_shortcuts(g_state)) running = false;

        // ── Fullscreen window ───────────────────────────────────────────
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGuiWindowFlags wflags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("##MainWindow", nullptr, wflags);

        float total_h = ImGui::GetContentRegionAvail().y;
        float content_h = total_h - STATUS_BAR_H;

        // ── Top content area (3 columns) ────────────────────────────────
        ImGui::BeginChild("ContentArea", ImVec2(0, content_h));

        // Left panel: file browser
        draw_file_browser(g_state);

        ImGui::SameLine();

        // Center panel: preview
        float center_w = ImGui::GetContentRegionAvail().x - RIGHT_PANEL_W;
        if (center_w < 100.0f) center_w = 100.0f;
        ImGui::BeginChild("CenterPanel", ImVec2(center_w, 0));
        draw_center_panel(g_state);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel: properties
        draw_properties_panel(g_state);

        ImGui::EndChild();

        // ── Status bar ──────────────────────────────────────────────────
        draw_status_bar(g_state);

        ImGui::End();

        // ── Render ──────────────────────────────────────────────────────
        ImGui::Render();
        int fb_w, fb_h;
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.08f, 0.08f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ── Cleanup ─────────────────────────────────────────────────────────
    free_preview_resources(g_state);

    if (g_state.fbo) av::delete_fbo(g_state.fbo, g_state.fbo_tex);
    if (g_state.checker_tex) glDeleteTextures(1, &g_state.checker_tex);

    av::audio_shutdown();
    av::renderer_shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
