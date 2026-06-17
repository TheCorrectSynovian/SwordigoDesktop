#ifndef GUI_H
#define GUI_H

#include <string>
#include <stdint.h>
#include <vector>
#include <functional>

// Menu action IDs returned by handle_click
enum GuiAction {
    GUI_NONE = 0,
    // File menu
    GUI_SAVE_STATE,
    GUI_LOAD_STATE,
    GUI_EXIT,
    // Emulation menu
    GUI_PAUSE,
    GUI_RESUME,
    GUI_RESTART,
    // Config menu
    GUI_CUSTOMIZE_CONTROLS,
    GUI_AUDIO_SETTINGS,
    GUI_TOGGLE_VSYNC,
    GUI_GFX_OPENGL,
    GUI_GFX_VULKAN,
    // Settings menu
    GUI_SCALE_UP,
    GUI_SCALE_DOWN,
    GUI_GAME_SPEED_UP,
    GUI_GAME_SPEED_DOWN,
    GUI_GAME_SPEED_RESET,
    GUI_TOGGLE_CAM,
    GUI_TOGGLE_PAUSE,
    GUI_TOGGLE_SMOOTH_CAM,
    // Help menu
    GUI_ABOUT,
    GUI_KEYBINDS,
    // Mod menu
    GUI_OPEN_MOD_MENU,
    GUI_MOD_PIN_TOGGLE,
    GUI_MOD_GOD_MODE,
    GUI_MOD_INFINITE_MANA,
    GUI_MOD_FLY_MODE,
    GUI_MOD_INFINITE_JUMP,
    GUI_MOD_COIN_BREAK,
    GUI_MOD_WALK_SPEED_UP,
    GUI_MOD_WALK_SPEED_DOWN,
    GUI_MOD_RUN_SPEED_UP,
    GUI_MOD_RUN_SPEED_DOWN,
    GUI_MOD_JUMP_HEIGHT_UP,
    GUI_MOD_JUMP_HEIGHT_DOWN,
    GUI_MOD_LEVEL_UP,
    GUI_MOD_LEVEL_DOWN,
    GUI_MOD_EXP_UP,
    GUI_MOD_EXP_DOWN,
};

struct MenuItem {
    std::string label;
    GuiAction action;
    bool enabled;
    MenuItem(const std::string& l, GuiAction a, bool e = true) : label(l), action(a), enabled(e) {}
};

struct Menu {
    std::string title;
    std::vector<MenuItem> items;
    float x, w; // Position and width in pixels
};

class GuiRenderer {
public:
    GuiRenderer();
    ~GuiRenderer();

    void init();
    void render(int mouse_x, int mouse_y, bool mouse_click, int win_w, int win_h);
    GuiAction handle_click(int mouse_x, int mouse_y, int win_w, int win_h);
    void draw_string(const std::string& str, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Returns true if any modal/panel/dropdown is open
    // When true, ALL clicks should be consumed by the GUI — don't pass to game
    bool has_modal_open() const { return show_about || show_settings || show_mod_menu || active_menu >= 0; }

    bool is_paused() const { return paused; }
    void set_paused(bool p) { paused = p; }

    // GUI scale factor (1.0 = default, 1.5 = 150%, etc.)
    float get_scale() const { return gui_scale; }
    void set_scale(float s) { gui_scale = s; if (gui_scale < 0.75f) gui_scale = 0.75f; if (gui_scale > 2.5f) gui_scale = 2.5f; }
    void scale_up() { set_scale(gui_scale + 0.25f); }
    void scale_down() { set_scale(gui_scale - 0.25f); }

    // Mod menu state (read by main.cpp to apply to game)
    bool mod_god_mode = false;
    bool mod_infinite_mana = false;
    bool mod_fly_mode = false;
    bool mod_infinite_jump = false;
    bool mod_coin_break = false;
    float mod_walk_speed = 1.0f;    // multiplier
    float mod_run_speed = 1.0f;
    float mod_jump_height = 1.0f;
    int mod_level = 0;              // 0 = don't override
    int mod_exp = 0;

    // Sidebar state
    bool mod_pinned = false;        // Pin to screen active
    bool mod_sidebar_open = false;  // Sidebar expanded

private:
    void draw_rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void draw_border(float x, float y, float w, float h, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void draw_char(char c, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Mod menu panel rendering & click helpers
    void render_mod_panel(float fwin_w, float fwin_h, int mouse_x, int mouse_y);
    void render_mod_sidebar(float fwin_w, float fwin_h, int mouse_x, int mouse_y);
    GuiAction handle_mod_panel_click(int mouse_x, int mouse_y, int win_w, int win_h);
    GuiAction handle_mod_sidebar_click(int mouse_x, int mouse_y, int win_w, int win_h);

    std::vector<Menu> menus;
    int active_menu = -1;
    int hover_menu = -1;
    int hover_item = -1;
    bool paused = false;
    bool show_about = false;
    bool show_settings = false;
    bool show_mod_menu = false;
    float gui_scale = 1.25f;       // Default: 125% scale

    // Base sizes (multiplied by gui_scale at render time)
    static const int BASE_BAR_H = 32;
    static const int BASE_ITEM_H = 28;
    static const int BASE_PAD = 10;
};

#endif
