#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdint.h>

// ============================================================
// Data-driven touch button — fully configurable via controls.ini
// ============================================================
struct TouchButton {
    // Identity
    std::string name;           // Internal ID: "left", "right", "jump", "magic_bolt", "custom_1"
    std::string display_name;   // User-facing: "Move Left", "Magic Bolt", "My Custom Btn"
    bool is_custom;             // User-created button?
    
    // Position & size (960x544 game coordinate space)
    float game_x, game_y;
    float radius;
    
    // Touch simulation
    int action_type;            // 1=TOUCH_DOWN, 2=TOUCH_UP, 4=TOUCH_MOVED
    int touch_id;               // Unique touch pointer ID for this button

    // Keyboard mapping (two keys per button)
    int sdl_scancode;           // Primary keyboard key (SDL_Scancode)
    int sdl_scancode_alt;       // Secondary keyboard key

    // Gamepad mapping
    int gamepad_button;         // SDL_GamepadButton (-1 = none)
    int gamepad_axis;           // SDL_GamepadAxis (-1 = none)
    float axis_threshold;       // Threshold for axis activation (0.5 default)

    // TV remote mapping
    int tv_remote_key;          // TV remote virtual key (-1 = none)

    // Magic menu macro — multi-step touch sequence
    // Step 1: TAP the magic button (opens spell menu — it's a toggle)
    // Step 2: after delay, TAP the target spell slot
    bool is_macro;
    int macro_open_touch_id;    // touch_id of the button to press first (e.g., magic)
    int macro_delay_ms;         // Delay between opener tap and slot tap (ms)
    float macro_target_x;      // Target touch position for step 2
    float macro_target_y;
    
    // Runtime state
    bool is_pressed;
    bool macro_pending;         // Macro sequence in progress
    int macro_stage;            // 0=idle, 1=opener pressed, 2=opener released/waiting, 3=done
    uint64_t macro_fire_time;   // Timestamp when sequence started

    TouchButton() : is_custom(false),
                    game_x(0), game_y(0), radius(40), action_type(1), touch_id(0),
                    sdl_scancode(0), sdl_scancode_alt(0), gamepad_button(-1),
                    gamepad_axis(-1), axis_threshold(0.5f), tv_remote_key(-1),
                    is_macro(false), macro_open_touch_id(-1), macro_delay_ms(250),
                    macro_target_x(0), macro_target_y(0),
                    is_pressed(false), macro_pending(false), macro_stage(0), macro_fire_time(0) {}
};

// Editor mode
enum EditorMode {
    EDITOR_POSITION = 0,    // Drag buttons to reposition
    EDITOR_REBIND,          // Click button → press key to rebind
    EDITOR_ADD,             // Adding a new custom button
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
    void toggle_editor() { editing_mode = !editing_mode; if (!editing_mode) editor_mode = EDITOR_POSITION; }
    void set_editing(bool e) { editing_mode = e; if (!e) editor_mode = EDITOR_POSITION; }
    EditorMode get_editor_mode() const { return editor_mode; }
    void set_editor_mode(EditorMode m) { editor_mode = m; }

    // Render overlays
    void render_touch_zones(int win_w, int win_h, bool show_labels = false);
    void render_editor(int win_w, int win_h, int mouse_x, int mouse_y, bool mouse_pressed);

    // Input handling
    int find_button_at(float game_x, float game_y);
    int find_by_scancode(int scancode);
    int find_by_gamepad_button(int button);
    int find_by_gamepad_axis(int axis, float value);

    // Get button info
    const std::vector<TouchButton>& get_buttons() const { return buttons; }
    TouchButton* get_button(int index);
    int button_count() const { return (int)buttons.size(); }

    // Editor handling
    void editor_mouse_down(float game_x, float game_y);
    void editor_mouse_move(float game_x, float game_y);
    void editor_mouse_up();
    
    // Key rebinding (called from event loop when in EDITOR_REBIND mode)
    // Returns true if the key was consumed by the editor
    bool editor_handle_key(int scancode);
    
    // Custom button management
    int add_custom_button(const std::string& display_name, float gx, float gy, int scancode);
    bool remove_button(int index);  // Only removes custom buttons
    
    // Macro processing — call every frame to fire pending macro step 2
    // touch_callback: function to send a touch event
    typedef std::function<void(int action, int id, double time, float x, float y, float ox, float oy, int tap)> TouchCallback;
    void process_macros(double current_time, TouchCallback callback);

    // Get SDL scancode name for display
    static std::string scancode_name(int scancode);

private:
    std::vector<TouchButton> buttons;
    bool editing_mode = false;
    EditorMode editor_mode = EDITOR_POSITION;
    int dragging_button = -1;
    float drag_offset_x = 0, drag_offset_y = 0;
    int rebind_target = -1;         // Button index being rebound
    int next_custom_touch_id = 40;  // Auto-assign touch IDs for custom buttons

    void draw_circle(float cx, float cy, float r, uint8_t red, uint8_t g, uint8_t b, uint8_t a, bool filled = true);
};

#endif
