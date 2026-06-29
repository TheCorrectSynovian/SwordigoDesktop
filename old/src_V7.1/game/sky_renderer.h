#pragma once
#define GL_GLEXT_PROTOTYPES
#include "platform/gl_inc.h"
#include <GL/glext.h>
#include <cmath>

// ============================================================
//  SkyRenderer — Dynamic procedural sky for SwordigoDesktop
//
//  Replaces the engine's baked background textures with a
//  dynamic sky featuring:
//    - Smooth gradient from horizon to zenith
//    - Sun disc with glow (day) / Moon disc (night)
//    - Procedural star field (night)
//    - Day/night cycle controlled by time_of_day
//
//  Rendered AFTER the game frame using a depth buffer trick:
//  draw at max depth so sky only shows where nothing was drawn.
// ============================================================

struct SkyColor {
    float r, g, b;
    
    SkyColor lerp(const SkyColor& other, float t) const {
        return {
            r + (other.r - r) * t,
            g + (other.g - g) * t,
            b + (other.b - b) * t
        };
    }
};

class SkyRenderer {
public:
    // Time of day: 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
    float time_of_day = 0.4f;  // Start at late morning
    float time_speed = 0.0f;   // 0 = frozen, 0.01 = slow cycle
    bool enabled = false;       // Disabled by default until hooks are confirmed
    
    // Sky palette presets
    struct SkyPreset {
        SkyColor zenith;     // Top of sky
        SkyColor horizon;    // Horizon line
        SkyColor sun_color;  // Sun/moon disc color
        float sun_size;      // Sun radius (0-1 range)
        float star_alpha;    // Star visibility (0-1)
    };
    
    void update(float dt) {
        time_of_day += time_speed * dt;
        if (time_of_day >= 1.0f) time_of_day -= 1.0f;
        if (time_of_day < 0.0f) time_of_day += 1.0f;
    }
    
    // Get interpolated sky colors for current time
    SkyPreset get_current_preset() const {
        // Define key times
        // 0.00 = midnight, 0.25 = sunrise, 0.50 = noon, 0.75 = sunset
        
        if (time_of_day < 0.20f) {
            // Night (midnight → pre-dawn)
            float t = time_of_day / 0.20f;
            return {
                {0.02f, 0.02f, 0.08f},   // Deep dark blue zenith
                {0.05f, 0.05f, 0.12f},   // Slightly lighter horizon
                {0.8f, 0.85f, 0.9f},     // Moon color
                0.02f,                    // Small moon
                1.0f - t * 0.5f          // Stars visible
            };
        } else if (time_of_day < 0.30f) {
            // Sunrise (pre-dawn → morning)
            float t = (time_of_day - 0.20f) / 0.10f;
            return {
                SkyColor{0.02f, 0.02f, 0.08f}.lerp({0.30f, 0.50f, 0.85f}, t),  // Dark → blue
                SkyColor{0.05f, 0.05f, 0.12f}.lerp({0.95f, 0.60f, 0.30f}, t),  // Dark → orange
                {1.0f, 0.85f, 0.4f},     // Warm sun
                0.04f + t * 0.02f,       // Growing sun
                (1.0f - t) * 0.5f        // Stars fading
            };
        } else if (time_of_day < 0.70f) {
            // Day (morning → afternoon)
            float t = (time_of_day - 0.30f) / 0.40f;
            float noon = 1.0f - fabsf(t - 0.5f) * 2.0f;  // 0→1→0 for noon peak
            return {
                SkyColor{0.30f, 0.50f, 0.85f}.lerp({0.40f, 0.65f, 0.95f}, noon),  // Blue → bright blue
                SkyColor{0.70f, 0.80f, 0.90f}.lerp({0.85f, 0.90f, 0.95f}, noon),  // Pale horizon
                {1.0f, 0.98f, 0.85f},    // White-yellow sun
                0.05f + noon * 0.02f,    // Bigger at noon
                0.0f                      // No stars
            };
        } else if (time_of_day < 0.80f) {
            // Sunset
            float t = (time_of_day - 0.70f) / 0.10f;
            return {
                SkyColor{0.35f, 0.50f, 0.85f}.lerp({0.15f, 0.10f, 0.25f}, t),  // Blue → purple
                SkyColor{0.70f, 0.75f, 0.85f}.lerp({0.90f, 0.40f, 0.15f}, t),  // Pale → orange/red
                {1.0f, 0.5f, 0.2f},      // Orange sun
                0.06f - t * 0.02f,       // Shrinking
                t * 0.3f                  // Stars appearing
            };
        } else {
            // Night (sunset → midnight)
            float t = (time_of_day - 0.80f) / 0.20f;
            return {
                SkyColor{0.15f, 0.10f, 0.25f}.lerp({0.02f, 0.02f, 0.08f}, t),  // Purple → dark
                SkyColor{0.20f, 0.12f, 0.15f}.lerp({0.05f, 0.05f, 0.12f}, t),  // Dim → dark
                {0.8f, 0.85f, 0.9f},     // Moon color
                0.02f,
                0.5f + t * 0.5f          // Full stars
            };
        }
    }
    
    // Render the sky — call this AFTER the game frame, with depth trick
    void render(int win_w, int win_h) {
        if (!enabled) return;
        
        SkyPreset sky = get_current_preset();
        
        // Save GL state
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, win_w, 0, win_h, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        
        // Depth trick: draw at max depth, only where game didn't draw
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);  // Pass where depth >= our depth (1.0 = clear value)
        glDepthMask(GL_FALSE);   // Don't write to depth buffer
        
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Draw gradient sky (full screen quad with vertex colors)
        float z = 0.999f;  // Near max depth
        glBegin(GL_QUADS);
        // Bottom (horizon)
        glColor3f(sky.horizon.r, sky.horizon.g, sky.horizon.b);
        glVertex3f(0, 0, z);
        glVertex3f(win_w, 0, z);
        // Top (zenith)
        glColor3f(sky.zenith.r, sky.zenith.g, sky.zenith.b);
        glVertex3f(win_w, win_h, z);
        glVertex3f(0, win_h, z);
        glEnd();
        
        // Draw sun/moon disc
        float sun_angle = time_of_day * 3.14159f * 2.0f - 1.5708f;  // -π/2 at midnight
        float sun_x = win_w * 0.5f + cosf(sun_angle) * win_w * 0.35f;
        float sun_y = win_h * 0.3f + sinf(sun_angle) * win_h * 0.5f;
        float sun_r = sky.sun_size * win_h;
        
        if (sun_y > -sun_r * 2) {  // Only draw if above horizon
            // Glow (larger, transparent)
            int segments = 24;
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(sky.sun_color.r, sky.sun_color.g, sky.sun_color.b, 0.3f);
            glVertex3f(sun_x, sun_y, z);
            glColor4f(sky.sun_color.r, sky.sun_color.g, sky.sun_color.b, 0.0f);
            for (int i = 0; i <= segments; i++) {
                float a = (float)i / segments * 6.2832f;
                glVertex3f(sun_x + cosf(a) * sun_r * 3.0f, 
                          sun_y + sinf(a) * sun_r * 3.0f, z);
            }
            glEnd();
            
            // Solid disc
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(sky.sun_color.r, sky.sun_color.g, sky.sun_color.b, 0.9f);
            glVertex3f(sun_x, sun_y, z);
            for (int i = 0; i <= segments; i++) {
                float a = (float)i / segments * 6.2832f;
                glVertex3f(sun_x + cosf(a) * sun_r, 
                          sun_y + sinf(a) * sun_r, z);
            }
            glEnd();
        }
        
        // Draw stars (night only)
        if (sky.star_alpha > 0.01f) {
            glPointSize(2.0f);
            glBegin(GL_POINTS);
            // Procedural star positions (deterministic based on index)
            for (int i = 0; i < 120; i++) {
                // Simple hash for pseudo-random positions
                float fx = ((i * 7919 + 1009) % 10000) / 10000.0f;
                float fy = ((i * 6271 + 3037) % 10000) / 10000.0f;
                float brightness = ((i * 4919 + 7717) % 1000) / 1000.0f;
                
                // Twinkle effect
                float twinkle = 0.6f + 0.4f * sinf(time_of_day * 100.0f + i * 1.7f);
                float alpha = sky.star_alpha * brightness * twinkle;
                
                glColor4f(0.9f, 0.92f, 1.0f, alpha);
                glVertex3f(fx * win_w, fy * win_h * 0.7f + win_h * 0.3f, z);
            }
            glEnd();
        }
        
        // Restore GL state
        glDepthMask(GL_TRUE);
        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glPopAttrib();
    }
};
