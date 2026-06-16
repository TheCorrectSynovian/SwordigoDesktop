#define GL_GLEXT_PROTOTYPES
#include "fbo_scaler.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <iostream>
#include <cstring>
#include <cmath>

// ============================================================
//  FBO + GLSL Upscaling Renderer Implementation
// ============================================================

static GLuint g_fbo       = 0;
static GLuint g_fbo_tex   = 0;
static GLuint g_fbo_depth = 0;
static int    g_game_w    = 960;
static int    g_game_h    = 544;
static bool   g_fbo_ok    = false;

// ----- GLSL Shader Sources -----------------------------------

// Shared vertex shader — full-screen quad
static const char* VERT_SRC = R"GLSL(
#version 120
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)GLSL";

// Sharp-bilinear — integer scales sharply, sub-pixel blends gently.
// Technique: scale UV to texel space, snap to nearest texel boundary,
// then blend only the fractional sub-texel portion.
static const char* FRAG_SHARP_BILINEAR = R"GLSL(
#version 120
uniform sampler2D u_tex;
uniform vec2 u_tex_size;   // e.g. 960.0, 544.0
uniform vec2 u_out_size;   // window size in pixels
varying vec2 v_uv;

void main() {
    vec2 scale    = u_out_size / u_tex_size;
    vec2 texel    = v_uv * u_tex_size;
    vec2 texel_f  = fract(texel);
    vec2 texel_i  = floor(texel) + 0.5;

    // Use a smooth transition only within the sub-texel fringe
    vec2 frange   = clamp(texel_f / fwidth(texel), 0.0, 1.0);
    vec2 uv_sharp = (texel_i + frange - 0.5) / u_tex_size;

    gl_FragColor  = texture2D(u_tex, uv_sharp);
}
)GLSL";

// Nearest-neighbor — raw pixels
static const char* FRAG_NEAREST = R"GLSL(
#version 120
uniform sampler2D u_tex;
uniform vec2 u_tex_size;
uniform vec2 u_out_size;
varying vec2 v_uv;
void main() {
    vec2 uv = (floor(v_uv * u_tex_size) + 0.5) / u_tex_size;
    gl_FragColor = texture2D(u_tex, uv);
}
)GLSL";

// CRT scanline — adds horizontal scanlines + slight barrel distortion
static const char* FRAG_CRT = R"GLSL(
#version 120
uniform sampler2D u_tex;
uniform vec2 u_tex_size;
uniform vec2 u_out_size;
varying vec2 v_uv;

vec2 barrel(vec2 uv) {
    vec2 c = uv - 0.5;
    float r2 = dot(c, c);
    return uv + c * r2 * 0.04;
}

void main() {
    vec2 uv  = barrel(v_uv);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 col = texture2D(u_tex, uv);

    // Scanlines — darken alternating rows at game resolution
    float scanline = sin(uv.y * u_tex_size.y * 3.14159) * 0.5 + 0.5;
    scanline = 0.75 + 0.25 * scanline;
    col.rgb *= scanline;

    // Vignette
    vec2 vig = uv * (1.0 - uv);
    float v  = vig.x * vig.y * 15.0;
    col.rgb *= clamp(v, 0.0, 1.0);

    gl_FragColor = col;
}
)GLSL";

// ----- Shader helpers ----------------------------------------

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(sh, 512, nullptr, buf);
        std::cerr << "[FBO] Shader compile error: " << buf << std::endl;
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(const char* vert, const char* frag) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_uv");
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, 512, nullptr, buf);
        std::cerr << "[FBO] Program link error: " << buf << std::endl;
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ----- Compiled programs (created lazily) --------------------

static GLuint g_prog_sharp  = 0;
static GLuint g_prog_nearest = 0;
static GLuint g_prog_crt     = 0;

static GLuint get_program(FBOScale mode) {
    switch (mode) {
        case FBOScale::NEAREST:
            if (!g_prog_nearest)
                g_prog_nearest = link_program(VERT_SRC, FRAG_NEAREST);
            return g_prog_nearest;
        case FBOScale::CRT_SCANLINE:
            if (!g_prog_crt)
                g_prog_crt = link_program(VERT_SRC, FRAG_CRT);
            return g_prog_crt;
        default: // SHARP_BILINEAR
            if (!g_prog_sharp)
                g_prog_sharp = link_program(VERT_SRC, FRAG_SHARP_BILINEAR);
            return g_prog_sharp;
    }
}

// ----- Public API --------------------------------------------

bool fbo_init(int game_w, int game_h) {
    g_game_w = game_w;
    g_game_h = game_h;

    // Create colour texture
    glGenTextures(1, &g_fbo_tex);
    glBindTexture(GL_TEXTURE_2D, g_fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, game_w, game_h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth-stencil renderbuffer
    glGenRenderbuffers(1, &g_fbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_fbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, game_w, game_h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create FBO
    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_fbo_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, g_fbo_depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[FBO] Framebuffer incomplete — status 0x"
                  << std::hex << status << std::dec << std::endl;
        g_fbo_ok = false;
        return false;
    }

    // Pre-compile the default shader immediately
    g_prog_sharp = link_program(VERT_SRC, FRAG_SHARP_BILINEAR);

    g_fbo_ok = true;
    std::cout << "[FBO] Framebuffer " << game_w << "x" << game_h
              << " ready — GLSL upscaling active" << std::endl;
    return true;
}

void fbo_destroy() {
    if (g_fbo)       { glDeleteFramebuffers(1, &g_fbo);       g_fbo = 0; }
    if (g_fbo_tex)   { glDeleteTextures(1, &g_fbo_tex);       g_fbo_tex = 0; }
    if (g_fbo_depth) { glDeleteRenderbuffers(1, &g_fbo_depth);g_fbo_depth = 0; }
    if (g_prog_sharp)  { glDeleteProgram(g_prog_sharp);  g_prog_sharp = 0; }
    if (g_prog_nearest){ glDeleteProgram(g_prog_nearest); g_prog_nearest = 0; }
    if (g_prog_crt)    { glDeleteProgram(g_prog_crt);     g_prog_crt = 0; }
    g_fbo_ok = false;
}

void fbo_begin_game() {
    if (!g_fbo_ok) return;
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glViewport(0, 0, g_game_w, g_game_h);
}

void fbo_end_game_and_blit(int win_w, int win_h, FBOScale mode) {
    if (!g_fbo_ok) return;

    // --- Unbind FBO, restore window framebuffer ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // When game res == window res, skip shader entirely — direct pixel-perfect blit
    if (g_game_w == win_w && g_game_h == win_h) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, g_game_w, g_game_h,
                          0, 0, win_w, win_h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    GLuint prog = get_program(mode);
    if (!prog) {
        // Fallback: blit with plain GL (no shader)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, g_game_w, g_game_h,
                          0, 0, win_w, win_h,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // --- Compute letterbox / pillarbox viewport ---
    float game_aspect = (float)g_game_w / (float)g_game_h;
    float win_aspect  = (float)win_w    / (float)win_h;
    int vx, vy, vw, vh;
    if (win_aspect > game_aspect) {
        // Pillarbox
        vh = win_h;
        vw = (int)(win_h * game_aspect);
        vx = (win_w - vw) / 2;
        vy = 0;
    } else {
        // Letterbox
        vw = win_w;
        vh = (int)(win_w / game_aspect);
        vx = 0;
        vy = (win_h - vh) / 2;
    }
    glViewport(vx, vy, vw, vh);

    // --- Save all old state ---
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLint old_prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_prog);

    // --- Draw full-screen quad with shader ---
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);

    glUseProgram(prog);

    // Uniforms
    glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
    glUniform2f(glGetUniformLocation(prog, "u_tex_size"),
                (float)g_game_w, (float)g_game_h);
    glUniform2f(glGetUniformLocation(prog, "u_out_size"),
                (float)vw, (float)vh);

    // Bind game texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_fbo_tex);
    // Use linear for shader — the shader handles sharpness itself
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Full-screen quad in NDC: pos (xy) + uv (st)
    // Note: UV Y is flipped because FBO is bottom-up
    static const float QUAD[] = {
        // x      y     u     v
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), QUAD);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), QUAD+2);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    // --- Restore state ---
    glUseProgram(old_prog);
    glPopAttrib();

    // Restore viewport to full window for GUI overlay
    glViewport(0, 0, win_w, win_h);
}

bool fbo_is_active() { return g_fbo_ok; }
unsigned int fbo_get_texture() { return g_fbo_tex; }
