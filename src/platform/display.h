#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include <stdint.h>
#include <string>

class Display {
public:
    Display();
    ~Display();
    
    bool init(int width, int height, const std::string& title);
    void swap();
    void poll_events();
    bool should_close() const { return quit; }
    
    int get_width() const { return width; }
    int get_height() const { return height; }
    
private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    int width = 800;
    int height = 480;
    bool quit = false;
};

#endif
