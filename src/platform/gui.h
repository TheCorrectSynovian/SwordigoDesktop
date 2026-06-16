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

    bool is_paused() const { return paused; }
    void set_paused(bool p) { paused = p; }

    // GUI scale factor (1.0 = default, 1.5 = 150%, etc.)
    float get_scale() const { return gui_scale; }
    void set_scale(float s) { gui_scale = s; if (gui_scale < 0.75f) gui_scale = 0.75f; if (gui_scale > 2.5f) gui_scale = 2.5f; }
    void scale_up() { set_scale(gui_scale + 0.25f); }
    void scale_down() { set_scale(gui_scale - 0.25f); }

private:
    void draw_rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void draw_border(float x, float y, float w, float h, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void draw_char(char c, float x, float y, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    std::vector<Menu> menus;
    int active_menu = -1;
    int hover_menu = -1;
    int hover_item = -1;
    bool paused = false;
    bool show_about = false;
    bool show_settings = false;
    float gui_scale = 1.25f;       // Default: 125% scale (bigger than before)

    // Base sizes (multiplied by gui_scale at render time)
    static const int BASE_BAR_H = 32;     // Menu bar height (was 28)
    static const int BASE_ITEM_H = 28;    // Dropdown item height (was 24)
    static const int BASE_PAD = 10;       // Padding (was 8)
};

#endif
