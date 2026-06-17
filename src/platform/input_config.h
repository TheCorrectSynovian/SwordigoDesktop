#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>

// Represents a virtual touch button that can be mapped to multiple input sources
struct TouchButton {
    std::string name;           // "left", "right", "jump", "attack", "magic", "use_item", "menu", "pause"
    float game_x, game_y;       // Center position in game coords (960x544)
    float radius;               // Touch radius in game coords
    int action_type;            // 1=TOUCH_DOWN, 2=TOUCH_UP, 4=TOUCH_MOVED
    int touch_id;               // Unique touch pointer ID for this button

    // Keyboard mapping
    int sdl_scancode;           // Primary keyboard key (SDL_Scancode)
    int sdl_scancode_alt;       // Alt keyboard key

    // Gamepad mapping
    int gamepad_button;         // SDL_GamepadButton (-1 = none)
    int gamepad_axis;           // SDL_GamepadAxis (-1 = none)
    float axis_threshold;       // Threshold for axis activation (0.5 default)

    // TV remote mapping (maps to arrow keys + enter/back)
    int tv_remote_key;          // TV remote virtual key (-1 = none)

    bool is_pressed;            // Current press state

    TouchButton() : game_x(0), game_y(0), radius(40), action_type(1), touch_id(0),
                    sdl_scancode(0), sdl_scancode_alt(0), gamepad_button(-1),
                    gamepad_axis(-1), axis_threshold(0.5f), tv_remote_key(-1),
                    is_pressed(false) {}
};

class InputConfig {
public:
    InputConfig();
    ~InputConfig();

    // Load/save controls config
    void load(const std::string& path);
    void save(const std::string& path);
    void load_defaults();

    // Editor mode
    bool is_editing() const { return editing_mode; }
    void toggle_editor() { editing_mode = !editing_mode; }
    void set_editing(bool e) { editing_mode = e; }

    // Render overlays
    void render_touch_zones(int win_w, int win_h, bool show_labels = false);
    void render_editor(int win_w, int win_h, int mouse_x, int mouse_y, bool mouse_pressed);

    // Input handling - returns game coords for matching button, or -1 if no match
    // These return the button index that was activated/deactivated
    int find_button_at(float game_x, float game_y);
    int find_by_scancode(int scancode);
    int find_by_gamepad_button(int button);
    int find_by_gamepad_axis(int axis, float value);

    // Get button info
    const std::vector<TouchButton>& get_buttons() const { return buttons; }
    TouchButton* get_button(int index);
    int button_count() const { return (int)buttons.size(); }

    // Editor drag handling
    void editor_mouse_down(float game_x, float game_y);
    void editor_mouse_move(float game_x, float game_y);
    void editor_mouse_up();

private:
    std::vector<TouchButton> buttons;
    bool editing_mode = false;
    int dragging_button = -1;
    float drag_offset_x = 0, drag_offset_y = 0;

    void draw_circle(float cx, float cy, float r, uint8_t red, uint8_t g, uint8_t b, uint8_t a, bool filled = true);
};

#endif
