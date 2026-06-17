#include "platform/display.h"
#include "platform/data_path.h"
#include <iostream>
#include "platform/gl_inc.h"
#include <SDL2/SDL_image.h>

Display::Display() {}

Display::~Display() {
#ifdef VULKAN_BACKEND
    // When using Vulkan, there is no GL context to delete.
    // VulkanBackend::destroy() handles Vulkan cleanup separately.
    if (!use_vulkan && gl_context) SDL_GL_DeleteContext(gl_context);
#else
    if (gl_context) SDL_GL_DeleteContext(gl_context);
#endif
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Display::init(int w, int h, const std::string& title) {
    width = w;
    height = h;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[Display] SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Request OpenGL Compatibility Profile for GLES 1.x fixed-function support
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "[Display] Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "[Display] GL context creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // VSync
    
    std::cout << "[Display] Window created: " << width << "x" << height << std::endl;
    std::cout << "[Display] GL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "[Display] GL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "[Display] GL Version:  " << glGetString(GL_VERSION) << std::endl;
    // Set window icon from icon_gnome.png
    {
        // Try via get_data_path (handles AppImage, RPM, DEB, dev)
        std::string icon_via_data = get_data_path("src/assets/icon_gnome.png");
        const char* icon_paths[] = {
            icon_via_data.c_str(),
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
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
            std::cout << "[Display] Window icon set" << std::endl;
        } else {
            std::cerr << "[Display] Could not load icon: " << IMG_GetError() << std::endl;
        }
    }

    return true;
}

void Display::swap() {
#ifdef VULKAN_BACKEND
    // VulkanBackend::end_frame_and_present() handles Vulkan presentation
    if (use_vulkan) return;
#endif
    SDL_GL_SwapWindow(window);
}

#ifdef VULKAN_BACKEND
bool Display::init_vulkan(int w, int h, const std::string& title) {
    width = w;
    height = h;
    use_vulkan = true;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[Display] SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "[Display] Vulkan window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // No GL context needed — VulkanBackend::init() creates the Vulkan device,
    // swapchain, etc. using this SDL window.

    std::cout << "[Display] Vulkan window created: " << width << "x" << height << std::endl;

    // Set window icon from icon_gnome.png
    {
        std::string icon_via_data = get_data_path("src/assets/icon_gnome.png");
        const char* icon_paths[] = {
            icon_via_data.c_str(),
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
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
            std::cout << "[Display] Window icon set" << std::endl;
        } else {
            std::cerr << "[Display] Could not load icon: " << IMG_GetError() << std::endl;
        }
    }

    return true;
}
#endif

void Display::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    quit = true;
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
                break;
        }
    }
}
