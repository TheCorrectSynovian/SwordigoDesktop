// =============================================================================
// Swordigo Desktop — ImGui Launcher UI
// Full Dear ImGui replacement for the old raw-OpenGL launcher.
// SDL3 + OpenGL 3.3 Core + ImGui v1.91.x
// =============================================================================

#include "platform/launcher_ui.h"
#include "platform/data_path.h"
#include "platform/save_editor.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#define GL_GLEXT_PROTOTYPES
#include "platform/gl_inc.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace fs = std::filesystem;

// =============================================================================
// Forward declarations (internal helpers)
// =============================================================================

static void      ApplyCustomTheme();
static GLuint    LoadTextureFromFile(const char* path, int* out_w, int* out_h);
static void      DrawToolbar(bool& running, LaunchConfig& cfg, bool& show_options);
static void      DrawInstancePanel(BinarySelector& selector, int& selected, float width);
static void      DrawDetailPanel(BinarySelector& selector, int selected,
                                 LaunchConfig& cfg, bool& running, int& api_sel,
                                 int& engine_sel, bool& show_save_editor, float mods_width);
static void      DrawModsPanel(float width);
static void      DrawStatusBar(int selected, const BinarySelector& selector);
static void      DrawOptionsModal(bool& show_options);
static void      DrawSaveEditor(bool& show_save_editor);

// =============================================================================
// Mod info (parsed from mod.json)
// =============================================================================

struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string type;      // "music", "scene", "texture"
    std::string dir_path;
    bool        enabled = true;
};

// Loaded UI textures
static GLuint g_tex_bg = 0;
static int    g_tex_bg_w = 0, g_tex_bg_h = 0;
static GLuint g_tex_icon_swordigo = 0;
static GLuint g_tex_icon_swmini = 0;
static GLuint g_tex_icon_rlswordigo = 0;
static GLuint g_tex_icon_app = 0;
static int    g_icon_w = 0, g_icon_h = 0; // reusable

// =============================================================================
// Module-level state (lives for the duration of show_launcher)
// =============================================================================

static ImFont* g_font_main    = nullptr;
static ImFont* g_font_heading = nullptr;

static std::vector<ModInfo> g_mods;
static bool g_mods_scanned = false;

// Save editor state
static bool                     g_save_loaded      = false;
static std::vector<std::string> g_save_paths;
static std::vector<SaveFile>    g_save_files;
static int                      g_save_sel         = -1;
static SaveFile                 g_edit_save;
static std::string              g_save_status;
static bool                     g_save_status_ok   = false;

// Delete confirmation
static bool g_confirm_delete    = false;
static int  g_delete_target_idx = -1;

// Add Instance popup state
static bool g_show_add_instance = false;
static char g_add_name[128] = "";
static int  g_add_asset_type = 0;        // 0=vanilla, 1=RL assets, 2=custom folder
static char g_add_custom_assets[512] = "";
static bool g_add_use_sre = true;
static char g_add_game_type[64] = "Swordigo";
static std::string g_add_status;
static bool g_add_copying = false;       // true during async asset copy
static float g_add_copy_progress = 0.0f; // 0.0-1.0 copy progress

// =============================================================================
// Texture helper
// =============================================================================

static GLuint LoadTextureFromFile(const char* path, int* out_w, int* out_h) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) return 0;

    // Convert to RGBA32
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surf);
    if (!rgba) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    if (out_w) *out_w = rgba->w;
    if (out_h) *out_h = rgba->h;

    SDL_DestroySurface(rgba);
    return tex;
}

// =============================================================================
// Theme
// =============================================================================

static void ApplyCustomTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding
    style.WindowRounding    = 0.0f;   // Full-window, no rounding on outer
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.TabRounding       = 6.0f;

    // Sizing
    style.FramePadding  = ImVec2(10, 6);
    style.ItemSpacing   = ImVec2(10, 8);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize   = 12.0f;

    // Borders
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;

    // Colors — #1a1a2e / #16213e / #0f3460 / #e94560 palette
    ImVec4* c = style.Colors;

    c[ImGuiCol_WindowBg]          = ImVec4(0.102f, 0.102f, 0.180f, 1.00f); // #1a1a2e
    c[ImGuiCol_ChildBg]           = ImVec4(0.059f, 0.204f, 0.376f, 0.30f); // #0f3460 @ 0.3
    c[ImGuiCol_PopupBg]           = ImVec4(0.086f, 0.129f, 0.243f, 0.95f); // #16213e
    c[ImGuiCol_Border]            = ImVec4(0.235f, 0.235f, 0.353f, 0.60f); // rgb(60,60,90)
    c[ImGuiCol_BorderShadow]      = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    c[ImGuiCol_FrameBg]           = ImVec4(0.086f, 0.129f, 0.243f, 1.00f); // #16213e
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.120f, 0.180f, 0.340f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.160f, 0.220f, 0.400f, 1.00f);

    c[ImGuiCol_TitleBg]           = ImVec4(0.070f, 0.070f, 0.140f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.102f, 0.102f, 0.180f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.070f, 0.070f, 0.140f, 0.50f);

    c[ImGuiCol_MenuBarBg]         = ImVec4(0.086f, 0.129f, 0.243f, 1.00f);

    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.050f, 0.050f, 0.100f, 0.50f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.235f, 0.235f, 0.353f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.300f, 0.300f, 0.450f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.400f, 0.400f, 0.550f, 1.00f);

    c[ImGuiCol_CheckMark]         = ImVec4(0.914f, 0.271f, 0.376f, 1.00f); // #e94560
    c[ImGuiCol_SliderGrab]        = ImVec4(0.914f, 0.271f, 0.376f, 0.80f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(1.000f, 0.349f, 0.455f, 1.00f);

    c[ImGuiCol_Button]            = ImVec4(0.914f, 0.271f, 0.376f, 1.00f); // #e94560
    c[ImGuiCol_ButtonHovered]     = ImVec4(1.000f, 0.349f, 0.455f, 1.00f); // #ff5974
    c[ImGuiCol_ButtonActive]      = ImVec4(0.784f, 0.196f, 0.275f, 1.00f); // #c83246

    c[ImGuiCol_Header]            = ImVec4(0.914f, 0.271f, 0.376f, 0.40f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.914f, 0.271f, 0.376f, 0.60f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.914f, 0.271f, 0.376f, 0.80f);

    c[ImGuiCol_Separator]         = ImVec4(0.235f, 0.235f, 0.353f, 0.50f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.914f, 0.271f, 0.376f, 0.60f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.914f, 0.271f, 0.376f, 1.00f);

    c[ImGuiCol_ResizeGrip]        = ImVec4(0.914f, 0.271f, 0.376f, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.914f, 0.271f, 0.376f, 0.60f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.914f, 0.271f, 0.376f, 0.95f);

    c[ImGuiCol_Tab]               = ImVec4(0.086f, 0.129f, 0.243f, 1.00f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.914f, 0.271f, 0.376f, 0.60f);
    c[ImGuiCol_TabSelected]       = ImVec4(0.914f, 0.271f, 0.376f, 0.80f);

    c[ImGuiCol_Text]              = ImVec4(0.933f, 0.933f, 0.933f, 1.00f); // #eeeeee
    c[ImGuiCol_TextDisabled]      = ImVec4(0.667f, 0.667f, 0.667f, 1.00f); // #aaaaaa

    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.086f, 0.129f, 0.243f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.235f, 0.235f, 0.353f, 0.80f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.235f, 0.235f, 0.353f, 0.40f);
    c[ImGuiCol_TableRowBg]        = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.000f, 1.000f, 1.000f, 0.03f);
}

// =============================================================================
// Mod scanner
// =============================================================================

static std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Minimal JSON value extractor (no external dependency)
static std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static void ScanMods() {
    g_mods.clear();
    std::string mods_dir = get_user_data_dir() + "/mods";
    if (!fs::exists(mods_dir) || !fs::is_directory(mods_dir)) {
        g_mods_scanned = true;
        return;
    }

    for (auto& entry : fs::directory_iterator(mods_dir)) {
        if (!entry.is_directory()) continue;
        
        std::string dirname = entry.path().filename().string();
        
        // Check if this is a disabled mod (dot-prefixed)
        bool is_disabled = (!dirname.empty() && dirname[0] == '.');
        
        std::string mod_json_path = entry.path().string() + "/mod.json";
        if (!fs::exists(mod_json_path)) continue;

        std::string json = ReadFileToString(mod_json_path);
        if (json.empty()) continue;

        ModInfo mod;
        mod.id          = JsonGetString(json, "id");
        mod.name        = JsonGetString(json, "name");
        mod.version     = JsonGetString(json, "version");
        mod.author      = JsonGetString(json, "author");
        mod.description = JsonGetString(json, "description");
        mod.type        = JsonGetString(json, "type");
        mod.dir_path    = entry.path().string();
        mod.enabled     = !is_disabled;

        if (mod.name.empty()) {
            // Strip dot prefix for display name
            mod.name = is_disabled ? dirname.substr(1) : dirname;
        }
        g_mods.push_back(mod);
    }

    g_mods_scanned = true;
}

// =============================================================================
// Human-readable file size
// =============================================================================

static std::string FormatFileSize(size_t bytes) {
    if (bytes < 1024)               return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024)        return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

// =============================================================================
// Toolbar
// =============================================================================

static void DrawToolbar(bool& running, LaunchConfig& cfg, bool& show_options) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 50));
    ImGui::Begin("##Toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Title
    if (g_font_heading) ImGui::PushFont(g_font_heading);
    ImGui::TextColored(ImVec4(0.914f, 0.271f, 0.376f, 1.0f), "\xe2\x9a\x94");
    ImGui::SameLine();
    ImGui::Text("SWORDIGO DESKTOP");
    if (g_font_heading) ImGui::PopFont();

    // Right-aligned buttons
    float rhs = ImGui::GetWindowWidth();

    // Close button
    ImGui::SameLine(rhs - 45);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("\xe2\x9c\x95", ImVec2(35, 35))) {
        cfg.should_launch = false;
        running = false;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close (ESC)");

    // Options button
    ImGui::SameLine(rhs - 90);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.45f, 1.0f));
    if (ImGui::Button("\xe2\x9a\x99", ImVec2(35, 35))) {
        show_options = true;
        ImGui::OpenPopup("Options");
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Options");

    ImGui::End();
}

// =============================================================================
// Icon helper — maps game_type to a loaded icon texture
// =============================================================================

static GLuint GetIconForInstance(const BinaryInfo& b) {
    if (b.game_type == "RLSwordigo" && g_tex_icon_rlswordigo) return g_tex_icon_rlswordigo;
    if (b.game_type == "SwordigoMini" && g_tex_icon_swmini) return g_tex_icon_swmini;
    if (g_tex_icon_swordigo) return g_tex_icon_swordigo;
    return g_tex_icon_app;
}

// =============================================================================
// Instance list panel (left)
// =============================================================================

static void DrawInstancePanel(BinarySelector& selector, int& selected, float width) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 panel_pos(vp->WorkPos.x, vp->WorkPos.y + 50);
    ImVec2 panel_size(width, vp->WorkSize.y - 50 - 28);

    ImGui::SetNextWindowPos(panel_pos);
    ImGui::SetNextWindowSize(panel_size);
    ImGui::Begin("##Instances", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header row: INSTANCES + Add button
    if (g_font_heading) ImGui::PushFont(g_font_heading);
    ImGui::TextColored(ImVec4(0.914f, 0.271f, 0.376f, 1.0f), "INSTANCES");
    if (g_font_heading) ImGui::PopFont();

    ImGui::SameLine(width - 50);
    if (ImGui::Button("+", ImVec2(30, 30))) {
        g_show_add_instance = true;
        // Reset form fields
        memset(g_add_name, 0, sizeof(g_add_name));
        strncpy(g_add_name, "My Instance", sizeof(g_add_name) - 1);
        memset(g_add_custom_assets, 0, sizeof(g_add_custom_assets));
        g_add_asset_type = 0;
        g_add_use_sre = true;
        strncpy(g_add_game_type, "Swordigo", sizeof(g_add_game_type) - 1);
        g_add_status.clear();
        g_add_copying = false;
        g_add_copy_progress = 0.0f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add new game instance");

    ImGui::Separator();

    // Instance list
    const auto& bins = selector.get_binaries();
    if (bins.empty()) {
        ImGui::TextDisabled("No instances found.");
        ImGui::TextDisabled("Click '+' to add one.");
    }

    // Build sorted index: ARM64 first, then by version descending
    std::vector<int> sorted_idx(bins.size());
    std::iota(sorted_idx.begin(), sorted_idx.end(), 0);
    std::sort(sorted_idx.begin(), sorted_idx.end(), [&bins](int a, int b_idx) {
        const auto& ba = bins[a];
        const auto& bb = bins[b_idx];
        // ARM64 before ARM32
        if (ba.arch != bb.arch) return ba.arch == BinaryArch::ARM64;
        // Higher version first
        return ba.version > bb.version;
    });

    // Initial selection: prefer ARM64 v1.4.12
    static bool first_frame = true;
    if (first_frame && !bins.empty()) {
        first_frame = false;
        for (int i = 0; i < (int)bins.size(); i++) {
            if (bins[i].arch == BinaryArch::ARM64 && bins[i].version.find("1.4.12") != std::string::npos) {
                selected = i;
                break;
            }
        }
    }

    for (int idx : sorted_idx) {
        const auto& b = bins[idx];
        ImGui::PushID(idx);

        bool is_selected = (selected == idx);

        // Custom selectable with extra info
        if (ImGui::Selectable("##instance", is_selected, 0, ImVec2(0, 56))) {
            selected = idx;
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem("InstanceCtx")) {
            if (ImGui::MenuItem("Set Default")) {
                selector.set_default(b.filepath);
            }
            if (ImGui::MenuItem("Open Folder")) {
                std::string dir = fs::path(get_user_data_dir() + "/" + b.filepath).parent_path().string();
                pid_t pid = fork();
                if (pid == 0) {
                    execlp("xdg-open", "xdg-open", dir.c_str(), nullptr);
                    _exit(1);
                }
            }
            ImGui::Separator();
            // Only show Remove for non-vanilla (custom) instances
            bool is_vanilla_ctx = (b.game_type == "Swordigo" || b.game_type == "RLSwordigo" || b.game_type == "SwordigoMini");
            if (!is_vanilla_ctx) {
                if (ImGui::MenuItem("Remove")) {
                    g_confirm_delete = true;
                    g_delete_target_idx = idx;
                }
            }
            ImGui::EndPopup();
        }

        // Overlay content on the selectable
        ImVec2 item_min = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Instance icon (40x40)
        GLuint icon = GetIconForInstance(b);
        if (icon) {
            dl->AddImageRounded(
                (ImTextureID)(intptr_t)icon,
                ImVec2(item_min.x + 4, item_min.y + 8),
                ImVec2(item_min.x + 44, item_min.y + 48),
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, 255), 6.0f);
        }

        // Status dot (shifted right by 44px for icon)
        ImVec4 dot_color;
        switch (b.status) {
            case BinaryStatus::TESTED:  dot_color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f); break;
            case BinaryStatus::TESTING: dot_color = ImVec4(0.9f, 0.9f, 0.3f, 1.0f); break;
            default:                    dot_color = ImVec4(0.7f, 0.3f, 0.3f, 1.0f); break;
        }
        dl->AddCircleFilled(
            ImVec2(item_min.x + 56, item_min.y + 14),
            5.0f, ImGui::ColorConvertFloat4ToU32(dot_color));

        // Name / label (shifted right by 44px for icon)
        dl->AddText(ImVec2(item_min.x + 68, item_min.y + 4),
            IM_COL32(238, 238, 238, 255), b.label.c_str());

        // Arch badge (shifted right by 44px for icon)
        ImVec4 badge_col = (b.arch == BinaryArch::ARM64)
            ? ImVec4(0.2f, 0.4f, 0.9f, 1.0f)    // blue
            : ImVec4(0.9f, 0.6f, 0.2f, 1.0f);    // orange
        const char* arch_str = BinarySelector::arch_string(b.arch);
        float badge_x = item_min.x + 68;
        float badge_y = item_min.y + 24;
        ImVec2 txt_size = ImGui::CalcTextSize(arch_str);
        dl->AddRectFilled(
            ImVec2(badge_x - 2, badge_y - 1),
            ImVec2(badge_x + txt_size.x + 4, badge_y + txt_size.y + 2),
            ImGui::ColorConvertFloat4ToU32(badge_col), 4.0f);
        dl->AddText(ImVec2(badge_x, badge_y),
            IM_COL32(255, 255, 255, 255), arch_str);

        // Version text next to badge (shifted right by 44px for icon)
        dl->AddText(ImVec2(badge_x + txt_size.x + 12, badge_y),
            IM_COL32(170, 170, 170, 255), b.version.c_str());

        // Assets badge
        if (b.assets_dir != "assets") {
            float ver_w = ImGui::CalcTextSize(b.version.c_str()).x;
            float ab_x = badge_x + txt_size.x + 12 + ver_w + 8;
            ImVec4 asset_col = (b.assets_dir == "rl_assets")
                ? ImVec4(0.8f, 0.2f, 0.4f, 1.0f)  // pink for RL
                : ImVec4(0.6f, 0.4f, 0.9f, 1.0f); // purple for custom
            std::string asset_tag = (b.assets_dir == "rl_assets") ? "[RL]" : "[Custom]";
            ImVec2 ab_size = ImGui::CalcTextSize(asset_tag.c_str());
            dl->AddRectFilled(
                ImVec2(ab_x - 2, badge_y - 1),
                ImVec2(ab_x + ab_size.x + 4, badge_y + ab_size.y + 2),
                ImGui::ColorConvertFloat4ToU32(asset_col), 4.0f);
            dl->AddText(ImVec2(ab_x, badge_y),
                IM_COL32(255, 255, 255, 255), asset_tag.c_str());
        }

        // Default star indicator
        if (b.is_default) {
            dl->AddText(ImVec2(item_min.x + width - 45, item_min.y + 4),
                IM_COL32(255, 215, 0, 255), "\xe2\x98\x85");
        }

        ImGui::PopID();
    }

    // Delete confirmation popup
    if (g_confirm_delete) {
        ImGui::OpenPopup("Confirm Delete");
        g_confirm_delete = false;
    }
    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove this instance?");
        ImGui::Separator();
        if (g_delete_target_idx >= 0 && g_delete_target_idx < (int)bins.size()) {
            ImGui::Text("  %s", bins[g_delete_target_idx].label.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("Remove", ImVec2(120, 0))) {
            selector.remove_instance(g_delete_target_idx);
            if (selected >= (int)selector.get_binaries().size()) {
                selected = std::max(0, (int)selector.get_binaries().size() - 1);
            }
            g_delete_target_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_delete_target_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// =============================================================================
// Detail panel (center)
// =============================================================================

static void DrawDetailPanel(BinarySelector& selector, int selected,
                            LaunchConfig& cfg, bool& running, int& api_sel,
                            int& engine_sel, bool& show_save_editor, float mods_width) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float inst_width = 250.0f;
    ImVec2 panel_pos(vp->WorkPos.x + inst_width, vp->WorkPos.y + 50);
    ImVec2 panel_size(vp->WorkSize.x - inst_width - mods_width, vp->WorkSize.y - 50 - 28);

    ImGui::SetNextWindowPos(panel_pos);
    ImGui::SetNextWindowSize(panel_size);
    ImGui::Begin("##Details", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // -- Save Editor sub-view --
    if (show_save_editor) {
        DrawSaveEditor(show_save_editor);
        ImGui::End();
        return;
    }

    const auto& bins = selector.get_binaries();
    if (bins.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No instance selected.");
        ImGui::TextDisabled("Add a game binary (.so) to get started.");
        ImGui::End();
        return;
    }

    if (selected < 0 || selected >= (int)bins.size()) {
        ImGui::End();
        return;
    }

    const BinaryInfo& b = bins[selected];

    // === Instance icon + name (heading) ===
    {
        // Large icon in detail view
        GLuint detail_icon = GetIconForInstance(b);
        if (detail_icon) {
            ImGui::Image((ImTextureID)(intptr_t)detail_icon, ImVec2(80, 80));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        if (g_font_heading) ImGui::PushFont(g_font_heading);
        ImGui::Text("%s", b.label.c_str());
        if (g_font_heading) ImGui::PopFont();

        // Arch badge inline
        {
            ImVec4 badge_col = (b.arch == BinaryArch::ARM64)
                ? ImVec4(0.2f, 0.4f, 0.9f, 1.0f) : ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, badge_col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, badge_col);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, badge_col);
            ImGui::SmallButton(BinarySelector::arch_string(b.arch));
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            const char* status_str;
            switch (b.status) {
                case BinaryStatus::TESTED:  status_str = "Stable"; break;
                case BinaryStatus::TESTING: status_str = "Testing"; break;
                default:                    status_str = "Unknown"; break;
            }
            ImGui::TextDisabled("(%s)", status_str);
        }
        ImGui::EndGroup();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // === Detail table ===
    if (ImGui::BeginTable("DetailsTable", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch);

        auto Row = [](const char* prop, const char* val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", prop);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(val);
        };

        Row("Version",   b.version.c_str());
        Row("Arch",      BinarySelector::arch_string(b.arch));
        Row("Game Type", b.game_type.c_str());
        Row("Assets",    b.assets_dir.c_str());
        Row("Path",      b.filepath.c_str());
        Row("Size",      FormatFileSize(b.file_size).c_str());
        Row("SHA256",    b.sha256.empty() ? "(not computed)" : b.sha256.substr(0, 16).c_str());

        const char* status_str;
        switch (b.status) {
            case BinaryStatus::TESTED:  status_str = "Tested (Stable)"; break;
            case BinaryStatus::TESTING: status_str = "Testing"; break;
            default:                    status_str = "Unknown"; break;
        }
        Row("Status", status_str);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // === LAUNCH button ===
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.914f, 0.271f, 0.376f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(1.000f, 0.349f, 0.455f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.784f, 0.196f, 0.275f, 1.0f));
    if (g_font_heading) ImGui::PushFont(g_font_heading);
    float launch_w = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("\xe2\x96\xb6  LAUNCH", ImVec2(launch_w, 50))) {
        cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
        cfg.use_dynarmic = (engine_sel == 1);
        cfg.selected_binary = b.filepath;
        cfg.assets_dir = b.assets_dir;
        cfg.game_type = b.game_type;
        cfg.should_launch = true;
        running = false;
    }
    if (g_font_heading) ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === Graphics API radio ===
    ImGui::Text("Graphics API:");
    ImGui::SameLine();
    ImGui::RadioButton("OpenGL", &api_sel, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Vulkan", &api_sel, 1);

    ImGui::Spacing();

    // === CPU Engine radio ===
    ImGui::Text("CPU Engine:");
    ImGui::SameLine();
    ImGui::RadioButton("Unicorn (TCG)", &engine_sel, 0);
    ImGui::SameLine();
#ifdef USE_DYNARMIC
    ImGui::RadioButton("Dynarmic (JIT)", &engine_sel, 1);
#else
    ImGui::BeginDisabled();
    ImGui::RadioButton("Dynarmic (JIT)", &engine_sel, 1);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Build with DYNARMIC=1 to enable");
    }
#endif

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === Action buttons row ===
    float btn_w = (ImGui::GetContentRegionAvail().x - 30) / 4.0f;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.086f, 0.129f, 0.243f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.150f, 0.220f, 0.400f, 1.0f));

    if (ImGui::Button("Save Editor", ImVec2(btn_w, 36))) {
        show_save_editor = true;
        // Load saves
        g_save_loaded = false;
        g_save_sel = -1;
        g_save_status.clear();
        std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
        std::string xdg = getenv("XDG_DATA_HOME") ? getenv("XDG_DATA_HOME") : (home + "/.local/share");
        std::string save_dir = xdg + "/swordigo-desktop/save/Documents";
        g_save_paths = save_list_dir(save_dir);
        g_save_files.clear();
        for (auto& p : g_save_paths) {
            SaveFile sf;
            if (save_load(p, sf)) {
                g_save_files.push_back(sf);
            }
        }
        g_save_loaded = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edit save files (.gplayer)");

    ImGui::SameLine();
    if (ImGui::Button("Open Folder", ImVec2(btn_w, 36))) {
        std::string dir = fs::path(get_user_data_dir() + "/" + b.filepath).parent_path().string();
        pid_t pid = fork();
        if (pid == 0) {
            execlp("xdg-open", "xdg-open", dir.c_str(), nullptr);
            _exit(1);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open instance folder in file manager");

    ImGui::SameLine();
    if (ImGui::Button("Asset Viewer", ImVec2(btn_w, 36))) {
        pid_t pid = fork();
        if (pid == 0) {
            // Try local build first, then installed path
            execlp("./asset_viewer", "asset_viewer", nullptr);
            execlp("asset_viewer", "asset_viewer", nullptr);
            _exit(1);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Browse game assets (textures, scenes)");

    // Only show Remove for non-vanilla (custom) instances
    {
        bool is_vanilla = (b.game_type == "Swordigo" || b.game_type == "RLSwordigo" || b.game_type == "SwordigoMini");
        if (!is_vanilla) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Remove", ImVec2(btn_w, 36))) {
                g_confirm_delete = true;
                g_delete_target_idx = selected;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this instance");
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::PopStyleColor(2);

    // === Dependencies section ===
    if (!b.dependencies.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Dependencies:");
        for (const auto& dep : b.dependencies) {
            ImGui::BulletText("%s", dep.c_str());
        }
    }

    ImGui::End();
}

// =============================================================================
// Mods panel (right)
// =============================================================================

static void DrawModsPanel(float width) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 panel_pos(vp->WorkPos.x + vp->WorkSize.x - width, vp->WorkPos.y + 50);
    ImVec2 panel_size(width, vp->WorkSize.y - 50 - 28);

    ImGui::SetNextWindowPos(panel_pos);
    ImGui::SetNextWindowSize(panel_size);
    ImGui::Begin("##Mods", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header
    if (g_font_heading) ImGui::PushFont(g_font_heading);
    ImGui::TextColored(ImVec4(0.914f, 0.271f, 0.376f, 1.0f), "MODS");
    if (g_font_heading) ImGui::PopFont();

    ImGui::SameLine(width - 120);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.086f, 0.129f, 0.243f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.150f, 0.220f, 0.400f, 1.0f));
    if (ImGui::Button("\xf0\x9f\x93\x81 Load Mod", ImVec2(105, 28))) {
        std::string mods_dir = get_user_data_dir() + "/mods";
        pid_t pid = fork();
        if (pid == 0) {
            execlp("xdg-open", "xdg-open", mods_dir.c_str(), nullptr);
            _exit(1);
        }
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open mods folder");

    ImGui::Separator();

    // Scan mods on first draw
    if (!g_mods_scanned) ScanMods();

    if (g_mods.empty()) {
        ImGui::Spacing(); ImGui::Spacing();
        float avail = ImGui::GetContentRegionAvail().x;
        float txt_w = ImGui::CalcTextSize("No mods installed").x;
        ImGui::SetCursorPosX((avail - txt_w) / 2.0f);
        ImGui::TextDisabled("No mods installed");

        ImGui::Spacing();
        txt_w = ImGui::CalcTextSize("Place mod folders with mod.json").x;
        ImGui::SetCursorPosX((avail - txt_w) / 2.0f);
        ImGui::TextDisabled("Place mod folders with mod.json");

        txt_w = ImGui::CalcTextSize("in ~/.local/share/swordigo-desktop/mods/").x;
        ImGui::SetCursorPosX((avail - txt_w) / 2.0f);
        ImGui::TextDisabled("in ~/.local/share/swordigo-desktop/mods/");
    } else {
        for (int i = 0; i < (int)g_mods.size(); i++) {
            auto& mod = g_mods[i];
            ImGui::PushID(i);

            bool prev_enabled = mod.enabled;
            ImGui::Checkbox("##enabled", &mod.enabled);
            
            // Toggle: rename folder to add/remove dot prefix
            if (mod.enabled != prev_enabled) {
                fs::path old_path(mod.dir_path);
                std::string dirname = old_path.filename().string();
                std::string new_dirname;
                
                if (!mod.enabled) {
                    // Disable: add dot prefix
                    if (dirname[0] != '.') {
                        new_dirname = "." + dirname;
                    }
                } else {
                    // Enable: remove dot prefix
                    if (dirname[0] == '.') {
                        new_dirname = dirname.substr(1);
                    }
                }
                
                if (!new_dirname.empty()) {
                    fs::path new_path = old_path.parent_path() / new_dirname;
                    std::error_code ec;
                    fs::rename(old_path, new_path, ec);
                    if (!ec) {
                        mod.dir_path = new_path.string();
                        std::cout << "[Launcher] Mod " 
                                  << (mod.enabled ? "enabled" : "disabled")
                                  << ": " << mod.name << " → " << new_dirname 
                                  << std::endl;
                    } else {
                        // Revert checkbox if rename failed
                        mod.enabled = prev_enabled;
                        std::cerr << "[Launcher] Failed to rename mod folder: " 
                                  << ec.message() << std::endl;
                    }
                }
            }
            
            ImGui::SameLine();

            ImGui::BeginGroup();
            if (!mod.enabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
            ImGui::Text("%s", mod.name.c_str());
            ImGui::TextDisabled("v%s by %s", mod.version.c_str(), mod.author.c_str());
            if (!mod.enabled) {
                ImGui::PopStyleColor();
            }
            ImGui::EndGroup();

            if (ImGui::IsItemHovered() && !mod.description.empty()) {
                ImGui::SetTooltip("%s\nType: %s\nStatus: %s", 
                    mod.description.c_str(), mod.type.c_str(),
                    mod.enabled ? "Enabled" : "Disabled");
            }

            ImGui::PopID();
        }
    }

    // Rescan button at bottom
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    ImGui::Separator();
    if (ImGui::Button("Rescan Mods", ImVec2(-1, 28))) {
        g_mods_scanned = false;
    }

    ImGui::End();
}

// =============================================================================
// Status bar
// =============================================================================

static void DrawStatusBar(int selected, const BinarySelector& selector) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 bar_pos(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - 28);
    ImVec2 bar_size(vp->WorkSize.x, 28);

    ImGui::SetNextWindowPos(bar_pos);
    ImGui::SetNextWindowSize(bar_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.060f, 0.060f, 0.120f, 1.0f));
    ImGui::Begin("##StatusBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Left: status
    const auto& bins = selector.get_binaries();
    if (!bins.empty() && selected >= 0 && selected < (int)bins.size()) {
        const char* status_str;
        switch (bins[selected].status) {
            case BinaryStatus::TESTED:  status_str = "Ready"; break;
            case BinaryStatus::TESTING: status_str = "Testing"; break;
            default:                    status_str = "Unknown status"; break;
        }
        ImGui::TextDisabled("%s", status_str);
    } else {
        ImGui::TextDisabled("Ready");
    }

    // Right: version + hints
    ImGui::SameLine(ImGui::GetWindowWidth() - 280);
    ImGui::TextDisabled("v5.0  |  Enter: Launch  |  ESC: Close  |  Del: Remove");

    ImGui::End();
    ImGui::PopStyleColor();
}

// =============================================================================
// Options modal
// =============================================================================

static void DrawOptionsModal(bool& show_options) {
    // Center the modal
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
               vp->WorkPos.y + vp->WorkSize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650, 500), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Options", &show_options,
            ImGuiWindowFlags_NoResize)) {
        return;
    }

    if (ImGui::BeginTabBar("OptionsTabs")) {
        // ─── SRE Hooks tab ───
        if (ImGui::BeginTabItem("SRE Hooks")) {
            ImGui::TextWrapped("All 34 SRE hooks are always active when using libsre.so.");
            ImGui::Spacing();

            if (ImGui::BeginTable("HooksTable", 3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                    ImVec2(0, 350))) {
                ImGui::TableSetupColumn("Hook Name",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Category",   ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Status",     ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                struct HookEntry { const char* name; const char* category; };
                static const HookEntry hooks[] = {
                    // CppString (4)
                    {"CppString::assign",            "CppString"},
                    {"CppString::append",            "CppString"},
                    {"CppString::c_str",             "CppString"},
                    {"CppString::destructor",        "CppString"},
                    // Lua (4)
                    {"lua_pcall",                    "Lua"},
                    {"luaL_loadbuffer",              "Lua"},
                    {"lua_newstate",                 "Lua"},
                    {"luaL_openlibs",                "Lua"},
                    // Background (3)
                    {"BackgroundLayer::render",      "Background"},
                    {"BackgroundLayer::update",      "Background"},
                    {"BackgroundLayer::setTexture",  "Background"},
                    // GUI (8)
                    {"GUI::render",                  "GUI"},
                    {"GUI::update",                  "GUI"},
                    {"GUI::handleInput",             "GUI"},
                    {"GUI::showDialog",              "GUI"},
                    {"GUI::hideDialog",              "GUI"},
                    {"GUI::showHUD",                 "GUI"},
                    {"GUI::hideHUD",                 "GUI"},
                    {"GUI::setButtonState",          "GUI"},
                    // Death (1)
                    {"Player::onDeath",              "Death"},
                    // Text Input (4)
                    {"TextInput::show",              "Text Input"},
                    {"TextInput::hide",              "Text Input"},
                    {"TextInput::getText",           "Text Input"},
                    {"TextInput::isActive",          "Text Input"},
                    // Music (7)
                    {"MusicPlayer::play",            "Music"},
                    {"MusicPlayer::stop",            "Music"},
                    {"MusicPlayer::pause",           "Music"},
                    {"MusicPlayer::resume",          "Music"},
                    {"MusicPlayer::setVolume",       "Music"},
                    {"MusicPlayer::isPlaying",       "Music"},
                    {"MusicPlayer::crossfade",       "Music"},
                    // Stats (1)
                    {"Stats::track",                 "Stats"},
                    // Menu (2)
                    {"MainMenu::show",               "Menu"},
                    {"MainMenu::handleSelection",    "Menu"},
                };

                for (const auto& h : hooks) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(h.name);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(h.category);
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Always On");
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        // ─── Graphics tab ───
        if (ImGui::BeginTabItem("Graphics")) {
            ImGui::Spacing();
            static bool postfx_enabled = true;
            ImGui::Checkbox("Enable PostFX (bloom, color grading)", &postfx_enabled);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Display Information:");
            int display_count = 0;
            SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
            if (displays && display_count > 0) {
                const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displays[0]);
                if (mode) {
                    ImGui::BulletText("Resolution: %dx%d", mode->w, mode->h);
                    ImGui::BulletText("Refresh Rate: %.1f Hz", mode->refresh_rate);
                }
                SDL_free(displays);
            } else {
                ImGui::TextDisabled("Could not query display info.");
            }

            ImGui::Spacing();
            ImGui::Text("OpenGL Info:");
            ImGui::BulletText("Renderer: %s", (const char*)glGetString(GL_RENDERER));
            ImGui::BulletText("Version:  %s", (const char*)glGetString(GL_VERSION));

            ImGui::EndTabItem();
        }

        // ─── About tab ───
        if (ImGui::BeginTabItem("About")) {
            ImGui::Spacing();
            if (g_font_heading) ImGui::PushFont(g_font_heading);
            ImGui::TextColored(ImVec4(0.914f, 0.271f, 0.376f, 1.0f),
                "Swordigo Desktop v5.0");
            if (g_font_heading) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::TextWrapped(
                "A desktop runtime for Swordigo, using ARM binary translation "
                "(FEX-Emu/box64) with custom SRE hooks for full playability.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Credits:");
            ImGui::BulletText("Touch Foo Games — original Swordigo");
            ImGui::BulletText("FEX-Emu — ARM64 translation");
            ImGui::BulletText("Dear ImGui — immediate-mode UI");
            ImGui::BulletText("SDL3 — cross-platform windowing & input");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Architecture:");
            ImGui::BulletText("SRT — Swordigo Runtime (overall architecture)");
            ImGui::BulletText("SRE — Swordigo Runtime Engine (libsre.so hooks)");
            ImGui::BulletText("Primary target: v1.4.12 ARM64");

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndPopup();
}

// =============================================================================
// Save editor (embedded in detail panel)
// =============================================================================

static void DrawSaveEditor(bool& show_save_editor) {
    // Back button
    if (ImGui::Button("\xe2\x86\x90 Back", ImVec2(100, 30))) {
        show_save_editor = false;
        g_save_sel = -1;
        return;
    }
    ImGui::SameLine();
    if (g_font_heading) ImGui::PushFont(g_font_heading);
    ImGui::Text("Save Editor");
    if (g_font_heading) ImGui::PopFont();

    ImGui::Separator();

    if (!g_save_loaded) {
        ImGui::TextDisabled("Loading saves...");
        return;
    }

    if (g_save_files.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No .gplayer save files found.");
        ImGui::TextDisabled("Save directory: ~/.local/share/swordigo-desktop/save/Documents/");
        return;
    }

    // ── Editing a specific save ──
    if (g_save_sel >= 0 && g_save_sel < (int)g_save_files.size()) {
        SaveFile& sf = g_edit_save;

        // Status message
        if (!g_save_status.empty()) {
            ImVec4 col = g_save_status_ok
                ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(col, "%s", g_save_status.c_str());
            ImGui::Spacing();
        }

        ImGui::Text("File: %s", fs::path(sf.filepath).filename().c_str());
        ImGui::Text("Player: %s  |  Level: %d  |  %.0f%% complete",
            sf.name.c_str(), sf.experience_level, sf.percent_completed * 100.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Character Stats:");
        ImGui::InputInt("Coins",  &sf.game_state.character.coins);
        ImGui::InputInt("Health", &sf.game_state.character.health);
        ImGui::InputInt("Mana",   &sf.game_state.character.mana);
        ImGui::InputInt("XP",     &sf.game_state.character.xp);
        ImGui::InputInt("Level",  &sf.game_state.character.level);

        ImGui::Spacing();
        ImGui::Text("Attributes:");
        ImGui::InputInt("Health Attr", &sf.game_state.character.health_attr);
        ImGui::InputInt("Attack Attr", &sf.game_state.character.attack_attr);
        ImGui::InputInt("Magic Attr",  &sf.game_state.character.magic_attr);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Apply / Back
        if (ImGui::Button("Apply & Save", ImVec2(160, 36))) {
            if (save_write(sf.filepath, sf)) {
                g_save_status = "Save written successfully!";
                g_save_status_ok = true;
                // Update the list entry
                g_save_files[g_save_sel] = sf;
            } else {
                g_save_status = "Failed to write save file!";
                g_save_status_ok = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 36))) {
            g_save_sel = -1;
            g_save_status.clear();
        }

        return;
    }

    // ── Save file list ──
    if (ImGui::BeginTable("SavesTable", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y - 10))) {
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Level",    ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("File",     ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)g_save_files.size(); i++) {
            auto& sf = g_save_files[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableNextColumn();
            if (ImGui::Selectable(sf.name.c_str(), false,
                    ImGuiSelectableFlags_SpanAllColumns)) {
                g_save_sel = i;
                g_edit_save = sf;  // Copy for editing
                g_save_status.clear();
            }
            ImGui::TableNextColumn();
            ImGui::Text("%d", sf.experience_level);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%%", sf.percent_completed * 100.0f);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", fs::path(sf.filepath).filename().c_str());

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// show_launcher — main entry point
// =============================================================================

LaunchConfig show_launcher(BinarySelector& selector) {
    LaunchConfig cfg;
    cfg.graphics_api = GraphicsAPI::OPENGL;
    cfg.should_launch = false;  // Default to not launching (user must click)

    // Reset module state
    g_font_main       = nullptr;
    g_font_heading    = nullptr;
    g_mods.clear();
    g_mods_scanned    = false;
    g_save_loaded     = false;
    g_save_paths.clear();
    g_save_files.clear();
    g_save_sel        = -1;
    g_save_status.clear();
    g_confirm_delete  = false;
    g_delete_target_idx = -1;

    // Pre-select default binary
    const auto& bins = selector.get_binaries();
    int bin_sel = 0;
    for (size_t i = 0; i < bins.size(); i++) {
        if (bins[i].is_default) bin_sel = (int)i;
    }

    // ── SDL3 init ──
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[Launcher] SDL_Init failed: " << SDL_GetError() << std::endl;
        return cfg;
    }

    // OpenGL 3.3 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,         0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,  SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,          1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,            24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,          8);

    SDL_Window* window = SDL_CreateWindow(
        "Swordigo Desktop",
        1200, 700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "[Launcher] Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return cfg;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Set window icon
    {
        std::string icon_via_data = get_data_path("src/assets/launcer_icon.png");
        const char* icon_paths[] = {
            icon_via_data.c_str(),
            "src/assets/launcer_icon.png",
            "src/assets/icon_gnome.png",
            "/usr/share/icons/hicolor/128x128/apps/swordigo-desktop.png",
            "/usr/share/pixmaps/swordigo-desktop.png",
            nullptr
        };
        for (int i = 0; icon_paths[i]; i++) {
            SDL_Surface* icon_surf = IMG_Load(icon_paths[i]);
            if (icon_surf) {
                SDL_SetWindowIcon(window, icon_surf);
                SDL_DestroySurface(icon_surf);
                std::cout << "[Launcher] Icon loaded from: " << icon_paths[i] << std::endl;
                break;
            }
        }
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "[Launcher] GL context failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return cfg;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Vsync

    // GL function loading is handled by ImGui's imgui_impl_opengl3_loader.h

    // ── ImGui init ──
    IMGUI_CHECKVERSION();
    ImGuiContext* imgui_ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Theme
    ApplyCustomTheme();

    // Font loading — try multiple paths
    {
        std::string font_paths[] = {
            get_user_data_dir() + "/src/assets/fonts/Inter-Regular.ttf",
            "src/assets/fonts/Inter-Regular.ttf",
        };
        bool font_loaded = false;
        for (auto& fp : font_paths) {
            if (fs::exists(fp)) {
                g_font_main    = io.Fonts->AddFontFromFileTTF(fp.c_str(), 16.0f);
                g_font_heading = io.Fonts->AddFontFromFileTTF(fp.c_str(), 24.0f);
                if (g_font_main && g_font_heading) {
                    font_loaded = true;
                    std::cout << "[Launcher] Font loaded from: " << fp << std::endl;
                    break;
                }
            }
        }
        if (!font_loaded) {
            std::cout << "[Launcher] Using ImGui default font" << std::endl;
            g_font_main    = io.Fonts->AddFontDefault();
            g_font_heading = g_font_main;
        }
    }

    // Platform/renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load UI textures — try multiple paths since assets may be in build tree or data dir
    {
        auto try_load = [](const char* sub, int* ow = nullptr, int* oh = nullptr) -> GLuint {
            int w = 0, h = 0;
            GLuint tex = 0;
            // Try 1: relative to CWD (dev mode: ~/SwordigoDesktop/)
            tex = LoadTextureFromFile((std::string("src/assets/") + sub).c_str(), &w, &h);
            // Try 2: user data dir (~/.local/share/swordigo-desktop/)
            if (!tex) {
                std::string p2 = get_user_data_dir() + "/assets/" + sub;
                tex = LoadTextureFromFile(p2.c_str(), &w, &h);
            }
            // Try 3: via get_data_path
            if (!tex) {
                std::string p3 = get_data_path(std::string("assets/") + sub);
                tex = LoadTextureFromFile(p3.c_str(), &w, &h);
            }
            if (ow) *ow = w;
            if (oh) *oh = h;
            return tex;
        };
        g_tex_bg = try_load("launcher_bg.png", &g_tex_bg_w, &g_tex_bg_h);
        g_tex_icon_swordigo = try_load("icons/swordigo_default.png");
        g_tex_icon_swmini = try_load("icons/swmini_default.png");
        g_tex_icon_rlswordigo = try_load("icons/rl_swordigo_default.png");
        g_tex_icon_app = try_load("icon_app.png");
        if (g_tex_bg) std::cout << "[LAUNCHER] Background texture loaded (" << g_tex_bg_w << "x" << g_tex_bg_h << ")" << std::endl;
        else std::cout << "[LAUNCHER] Warning: Background texture not found" << std::endl;
    }

    // ── Main loop ──
    bool running = true;
    int api_sel = 0;        // 0 = OpenGL, 1 = Vulkan
    int engine_sel = 1;     // 0 = Unicorn, 1 = Dynarmic (default: JIT for performance)
    bool show_options = false;
    bool show_save_editor = false;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type) {
                case SDL_EVENT_QUIT:
                    cfg.should_launch = false;
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        cfg.should_launch = false;
                        running = false;
                    }
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                        // Launch the selected binary
                        const auto& cur_bins = selector.get_binaries();
                        if (!cur_bins.empty() && bin_sel >= 0 && bin_sel < (int)cur_bins.size()) {
                            cfg.graphics_api = (api_sel == 0) ? GraphicsAPI::OPENGL : GraphicsAPI::VULKAN;
                            cfg.use_dynarmic = (engine_sel == 1);
                            cfg.selected_binary = cur_bins[bin_sel].filepath;
                            cfg.assets_dir = cur_bins[bin_sel].assets_dir;
                            cfg.game_type = cur_bins[bin_sel].game_type;
                            cfg.should_launch = true;
                            running = false;
                        }
                    }
                    else if (event.key.key == SDLK_DELETE) {
                        const auto& cur_bins = selector.get_binaries();
                        if (!cur_bins.empty() && bin_sel >= 0 && bin_sel < (int)cur_bins.size()) {
                            g_confirm_delete = true;
                            g_delete_target_idx = bin_sel;
                        }
                    }
                    break;
            }
        }

        if (!running) break;

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Draw all panels
        float inst_panel_w = 250.0f;
        float mods_panel_w = 280.0f;

        // Draw background texture
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            if (g_tex_bg) {
                // Draw background image at moderate opacity
                ImGui::GetBackgroundDrawList()->AddImage(
                    (ImTextureID)(intptr_t)g_tex_bg,
                    ImVec2(vp->WorkPos.x, vp->WorkPos.y),
                    ImVec2(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y),
                    ImVec2(0, 0), ImVec2(1, 1),
                    IM_COL32(255, 255, 255, 90)  // ~35% opacity — visible but not overwhelming
                );
                // Darken overlay gradient (top-to-bottom) for readability
                ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
                    ImVec2(vp->WorkPos.x, vp->WorkPos.y),
                    ImVec2(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y),
                    IM_COL32(26, 26, 46, 180),    // top: dark
                    IM_COL32(26, 26, 46, 180),    // top-right
                    IM_COL32(26, 26, 46, 220),    // bottom-right: darker
                    IM_COL32(26, 26, 46, 220)     // bottom-left
                );
            }
        }

        DrawToolbar(running, cfg, show_options);
        DrawInstancePanel(selector, bin_sel, inst_panel_w);
        DrawDetailPanel(selector, bin_sel, cfg, running, api_sel, engine_sel, show_save_editor, mods_panel_w);
        DrawModsPanel(mods_panel_w);
        DrawStatusBar(bin_sel, selector);

        // Options modal (drawn on top)
        if (show_options) {
            DrawOptionsModal(show_options);
        }

        // Add Instance popup
        if (g_show_add_instance) {
            ImGui::OpenPopup("Add Instance");
        }
        ImVec2 popup_center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(popup_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Add Instance", &g_show_add_instance, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

            // ── INSTANCE DETAILS ──
            ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "INSTANCE DETAILS");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Instance Name");
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##inst_name", g_add_name, sizeof(g_add_name));

            // Game Type
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Game Type");
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(-1);
            const char* game_types[] = { "Swordigo", "RLSwordigo", "Custom" };
            static int gt_sel = 0;
            if (ImGui::Combo("##game_type", &gt_sel, game_types, 3)) {
                strncpy(g_add_game_type, game_types[gt_sel], sizeof(g_add_game_type) - 1);
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // ── ASSETS ──
            ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "ASSETS");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Asset Source");
            ImGui::SameLine(160);
            ImGui::RadioButton("Vanilla (assets/)", &g_add_asset_type, 0);
            ImGui::SameLine();
            ImGui::RadioButton("RL (rl_assets/)", &g_add_asset_type, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Custom folder", &g_add_asset_type, 2);

            if (g_add_asset_type == 2) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Folder Path");
                ImGui::SameLine(160);
                ImGui::SetNextItemWidth(-70);
                ImGui::InputText("##custom_assets", g_add_custom_assets, sizeof(g_add_custom_assets));
                ImGui::SameLine();
                if (ImGui::Button("Browse##ca", ImVec2(60, 0))) {
                    std::string data_dir = get_user_data_dir();
                    pid_t pid = fork();
                    if (pid == 0) {
                        execlp("xdg-open", "xdg-open", data_dir.c_str(), nullptr);
                        _exit(1);
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // ── ENGINE (collapsed by default, advanced) ──
            if (ImGui::CollapsingHeader("Engine (Advanced)")) {
                ImGui::Indent(12);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Use SRE");
                ImGui::SameLine(160);
                ImGui::Checkbox("##use_sre", &g_add_use_sre);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Swordigo Runtime Engine (libsre.so)");
                    ImGui::Text("Provides hooks, fixes, and mod support.");
                    ImGui::Text("Required for mods, save editing, and custom content.");
                    ImGui::EndTooltip();
                }

                if (g_add_use_sre) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
                    ImGui::TextWrapped("  SRE replaces libmini.so / libGlossHook.so");
                    ImGui::PopStyleColor();
                }
                ImGui::Unindent(12);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Status message
            if (!g_add_status.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", g_add_status.c_str());
                ImGui::Spacing();
            }

            // Action buttons
            float btn_w = 120;
            float total_w = btn_w * 2 + 8;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_w) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.34f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.40f, 1.0f));
            if (ImGui::Button("Create", ImVec2(btn_w, 36))) {
                g_add_status.clear();
                // Validate
                if (strlen(g_add_name) == 0) {
                    g_add_status = "Instance name is required.";
                } else {
                    // Determine assets_dir
                    std::string assets_dir_name;
                    if (g_add_asset_type == 0) {
                        assets_dir_name = "assets";
                    } else if (g_add_asset_type == 1) {
                        assets_dir_name = "rl_assets";
                    } else {
                        // Custom: copy folder to inst-<name>/
                        std::string custom_src = g_add_custom_assets;
                        if (custom_src.empty()) {
                            g_add_status = "Please specify the custom assets folder.";
                        } else {
                            assets_dir_name = std::string("inst-") + g_add_name;
                            std::string dest = get_user_data_dir() + "/" + assets_dir_name;
                            try {
                                if (fs::exists(dest)) {
                                    fs::remove_all(dest);
                                }
                                fs::create_directories(dest);
                                // Copy entire assets folder
                                fs::copy(custom_src, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                std::cout << "[Launcher] Copied assets: " << custom_src << " -> " << dest << std::endl;
                            } catch (const std::exception& e) {
                                g_add_status = std::string("Error copying assets: ") + e.what();
                            }
                        }
                    }

                    if (g_add_status.empty()) {
                        // The binary is ALWAYS v1.4.12 ARM64
                        std::string base_so = get_user_data_dir() + "/engine/v1.4.12/arm64-v8a/libswordigo.so";

                        // Use add_custom_instance which copies .so and registers
                        if (selector.add_custom_instance(base_so, g_add_name, assets_dir_name)) {
                            // If SRE enabled, ensure libsre.so is copied to the new instance dir
                            if (g_add_use_sre) {
                                std::string sre_src = get_user_data_dir() + "/engine/v1.4.12/arm64-v8a/libsre.so";
                                std::string arch_dir = get_user_data_dir() + "/engine/custom-" + std::string(g_add_name) + "/arm64-v8a";
                                if (fs::exists(sre_src)) {
                                    try {
                                        fs::copy_file(sre_src, arch_dir + "/libsre.so", fs::copy_options::overwrite_existing);
                                    } catch (...) {}
                                }

                                // Auto-strip libmini.so and libGlossHook.so from deps
                                auto& bins = const_cast<std::vector<BinaryInfo>&>(selector.get_binaries());
                                if (!bins.empty()) {
                                    auto& last = bins.back();
                                    // Remove libmini.so and libGlossHook.so from dependencies
                                    auto& deps = last.dependencies;
                                    deps.erase(std::remove_if(deps.begin(), deps.end(), [](const std::string& d) {
                                        return d == "libmini.so" || d == "libGlossHook.so";
                                    }), deps.end());
                                    auto& dpaths = last.dep_paths;
                                    dpaths.erase(std::remove_if(dpaths.begin(), dpaths.end(), [](const std::string& p) {
                                        return p.find("libmini.so") != std::string::npos ||
                                               p.find("libGlossHook.so") != std::string::npos;
                                    }), dpaths.end());
                                }
                            }

                            // Save user instances for persistence
                            std::string config_dir;
                            const char* xdg_config = getenv("XDG_CONFIG_HOME");
                            if (xdg_config) {
                                config_dir = std::string(xdg_config) + "/swordigo-desktop";
                            } else {
                                const char* home = getenv("HOME");
                                config_dir = std::string(home ? home : ".") + "/.config/swordigo-desktop";
                            }
                            selector.save_user_instances(config_dir + "/instances.json");

                            g_show_add_instance = false;
                            ImGui::CloseCurrentPopup();
                        } else {
                            g_add_status = "Failed to create instance.";
                        }
                    }
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(btn_w, 36))) {
                g_show_add_instance = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }

        // Render
        ImGui::Render();
        int fb_w, fb_h;
        SDL_GetWindowSizeInPixels(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.102f, 0.102f, 0.180f, 1.0f); // Match WindowBg
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // ── Cleanup ──
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(imgui_ctx);

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Ensure we return a valid config if launching
    if (cfg.should_launch) {
        const auto& final_bins = selector.get_binaries();
        if (cfg.selected_binary.empty() && !final_bins.empty() && bin_sel >= 0) {
            cfg.selected_binary = final_bins[bin_sel].filepath;
            cfg.assets_dir = final_bins[bin_sel].assets_dir;
            cfg.game_type = final_bins[bin_sel].game_type;
        }
    }

    return cfg;
}
