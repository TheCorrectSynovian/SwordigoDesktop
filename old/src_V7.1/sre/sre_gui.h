/* sre_gui.h — GUI subsystem for libsre.so
 *
 * Provides access to the Caver engine's entire GUI rendering stack:
 *   - RenderingContext drawing API (rects, textures, sprites)
 *   - TextureLibrary (load game textures by name)
 *   - FontText (text rendering with game fonts)
 *   - GUIView hierarchy (create/manipulate views)
 *   - GameInterfaceBuilder (factory for styled buttons, labels, etc.)
 *   - GUIWindow (root window, modal presentation)
 *   - GUIAlertView (modal dialogs)
 *
 * nm symbol addresses below are for v1.4.12 ARM64.
 */

#ifndef SRE_GUI_H
#define SRE_GUI_H

#include "sre.h"

/* ========== Type aliases for readability ========== */

/* Caver::Rectangle — 4 floats: x, y, width, height */
typedef struct { float x, y, width, height; } SreRect;

/* Caver::Vector2 — 2 floats */
typedef struct { float x, y; } SreVec2_gui;

/* boost::shared_ptr<T> — 16 bytes: raw pointer + shared_count* */
typedef struct {
    sre_u64 ptr;            /* T* raw pointer */
    sre_u64 shared_count;   /* boost::detail::shared_count* */
} SreSharedPtr;

/* ========== Types from sre_background.c ========== */
typedef void (*pfn_SetMatrix)(void* ctx, void* matrix);
typedef void (*pfn_SetColor)(void* ctx, void* color);
typedef void (*pfn_SpriteDraw)(void* sprite, void* ctx);

/* ========== RenderingContext ========== */
typedef void  (*pfn_SetProjectionMatrix)(void* ctx, void* matrix);
typedef void  (*pfn_FillRect)(void* ctx, void* rect, void* color, float alpha);
typedef void  (*pfn_DrawRect_RC)(void* ctx, void* rect, void* color, float alpha);
typedef void  (*pfn_SetBlendingEnabled)(void* ctx, int enabled);
typedef void  (*pfn_SetTexturingEnabled)(void* ctx, int enabled);
typedef void  (*pfn_SetLightingEnabled)(void* ctx, int enabled);
typedef void  (*pfn_SetDepthTestEnabled)(void* ctx, int enabled);
typedef void  (*pfn_SetDepthWriteEnabled)(void* ctx, int enabled);
typedef void  (*pfn_SetAlpha)(void* ctx, float alpha);
typedef void  (*pfn_BindTexture)(void* ctx, void* texture);
typedef void  (*pfn_DrawArrays)(void* ctx, int mode, int first, int count);
typedef void  (*pfn_UseProgram)(void* ctx, unsigned int programId);
typedef void  (*pfn_SetVertexAttribPointer)(void* ctx, unsigned int type,
                                            int size, int dataType,
                                            int stride, void* data);

/* ========== FontText (0x4c____ range) ========== */
typedef void  (*pfn_FontText_Draw)(void* fontText, void* ctx);         /* 0x4c66dc */
typedef void  (*pfn_FontText_AddText)(void* fontText, void* str,       /* 0x4c53e4 */
                                      float fontSize, void* pos);
typedef void  (*pfn_FontText_Clear)(void* fontText);                   /* 0x4c5124 */
typedef void  (*pfn_FontText_SetColor)(void* fontText, void* color);   /* 0x4c5170 */
typedef void  (*pfn_FontText_Translate)(void* fontText, void* vec2);   /* 0x4c6210 */
typedef void  (*pfn_FontText_AlignH)(void* fontText, int alignment);   /* 0x4c6280 */

/* ========== GUILabel (0x497___ range) ========== */
typedef void  (*pfn_GUILabel_AddText)(void* label, void* str);              /* 0x497fdc */
typedef void  (*pfn_GUILabel_AddTextColor)(void* label, void* str,          /* 0x4980fc */
                                           void* color);
typedef void  (*pfn_GUILabel_UpdateText)(void* label);                      /* 0x497b54 */

/* ========== GUIButton (0x495___ range) ========== */
typedef void  (*pfn_GUIButton_Ctor)(void* btn, int buttonType);             /* 0x4950a4 */
typedef void  (*pfn_GUIButton_SetTitle)(void* btn, void* str);              /* 0x495a74 */
typedef void  (*pfn_GUIButton_SetImage)(void* btn, void* str);              /* 0x495b00 */
typedef void* (*pfn_GUIButton_titleLabel)(void* btn);                       /* 0x33f9e4 */

/* ========== GUIView (0x49f___ range) ========== */
typedef void  (*pfn_GUIView_SetFrame)(void* view, void* rect);             /* 0x49f5c8 */
typedef void  (*pfn_GUIView_AddSubview)(void* parent, void* sharedPtr);    /* 0x49f6ec */
typedef void  (*pfn_GUIView_RemoveFromSuperview)(void* view);              /* 0x49fa60 */

/* ========== GUIWindow (0x4a2___ range) ========== */
typedef void* (*pfn_GUIWindow_mainWindow)(void);                           /* 0x4a21ec */
typedef void  (*pfn_GUIWindow_PresentModal)(void* win, void* sharedPtr,    /* 0x4a31b4 */
                                             int animated);
typedef void  (*pfn_GUIWindow_DismissModal)(void* win, void* view);        /* 0x4a3540 */
typedef void  (*pfn_GUIWindow_Dismiss)(void* win);                         /* 0x4a3590 */

/* ========== GUIAlertView (0x490___ range) ========== */
typedef void  (*pfn_GUIAlertView_SetTitle)(void* alert, void* str);        /* 0x490628 */
typedef void  (*pfn_GUIAlertView_SetMessage)(void* alert, void* str);      /* 0x490850 */
typedef void  (*pfn_GUIAlertView_AddButton)(void* alert, void* sharedPtr); /* 0x490bf4 */

/* ========== GameInterfaceBuilder (0x33e-0x342 range) ========== */
/* Static factory methods — return shared_ptr via hidden first arg (ARM64 ABI) */
typedef void (*pfn_GIB_NormalLabel)(void* result, void* str,       /* 0x33ea24 */
                                    void* color1, void* color2);
typedef void (*pfn_GIB_SmallLabel)(void* result, void* str,        /* 0x33ee2c */
                                   void* color1, void* color2);
typedef void (*pfn_GIB_FramedButton)(void* result, void* str,      /* 0x33ff98 */
                                      int hasBorder);
typedef void (*pfn_GIB_MainMenuButton)(void* result, void* str);   /* 0x340504 */
typedef void (*pfn_GIB_AlertView)(void* result, void* title,       /* 0x3408e0 */
                                   void* message, int type,
                                   void* buttons, int btnCount);
typedef void (*pfn_GIB_Slider)(void* result, float min, float max);/* 0x341108 */
typedef void (*pfn_GIB_Switch)(void* result);                      /* 0x34188c */

/* ========== TextureLibrary (0x4cc___ range) ========== */
typedef void* (*pfn_TextureLibrary_shared)(void);                  /* 0x4cc308 */
typedef void  (*pfn_TextureLibrary_ForName)(void* result,          /* 0x4cc494 */
                                            void* lib, void* str, int flag);

/* ========== FontLibrary (0x4c3-0x4c4 range) ========== */
typedef void* (*pfn_FontLibrary_shared)(void);                     /* 0x4c33b0 */
typedef void  (*pfn_FontLibrary_DefaultFont)(void* result,         /* 0x4c4364 */
                                              void* lib);
typedef void  (*pfn_FontLibrary_SmallFont)(void* result,           /* 0x4c44a8 */
                                            void* lib);

/* ========== Init struct — passed from host ========== */
typedef struct {
    /* RenderingContext extended API (14 entries) */
    sre_u64 SetProjectionMatrix;     /* [0]  0x48b184 */
    sre_u64 FillRect;                /* [1]  0x48c6a0 */
    sre_u64 DrawRect;                /* [2]  0x48c9f8 */
    sre_u64 SetBlendingEnabled;      /* [3]  0x48b8d8 */
    sre_u64 SetTexturingEnabled;     /* [4]  0x48b900 */
    sre_u64 SetLightingEnabled;      /* [5]  0x48b934 */
    sre_u64 SetDepthTestEnabled;     /* [6]  0x48b990 */
    sre_u64 SetDepthWriteEnabled;    /* [7]  0x48b9b8 */
    sre_u64 SetAlpha;                /* [8]  0x48b4bc */
    sre_u64 BindTexture;             /* [9]  0x48bfa4 */
    sre_u64 DrawArrays;              /* [10] 0x48c330 */
    sre_u64 UseProgram;              /* [11] 0x48bcec */
    sre_u64 SetVertexAttribPointer;  /* [12] 0x48c2bc */
    sre_u64 _pad_rc;                 /* [13] reserved */
    
    /* FontText (6 entries) */
    sre_u64 FontText_Draw;           /* [14] 0x4c66dc */
    sre_u64 FontText_AddText;        /* [15] 0x4c53e4 */
    sre_u64 FontText_Clear;          /* [16] 0x4c5124 */
    sre_u64 FontText_SetColor;       /* [17] 0x4c5170 */
    sre_u64 FontText_Translate;      /* [18] 0x4c6210 */
    sre_u64 FontText_AlignH;         /* [19] 0x4c6280 */
    
    /* GUILabel (3 entries) */
    sre_u64 GUILabel_AddText;        /* [20] 0x497fdc */
    sre_u64 GUILabel_AddTextColor;   /* [21] 0x4980fc */
    sre_u64 GUILabel_UpdateText;     /* [22] 0x497b54 */
    
    /* GUIButton (4 entries) */
    sre_u64 GUIButton_Ctor;          /* [23] 0x4950a4 */
    sre_u64 GUIButton_SetTitle;      /* [24] 0x495a74 */
    sre_u64 GUIButton_SetImage;      /* [25] 0x495b00 */
    sre_u64 GUIButton_titleLabel;    /* [26] 0x33f9e4 */
    
    /* GUIView (3 entries) */
    sre_u64 GUIView_SetFrame;        /* [27] 0x49f5c8 */
    sre_u64 GUIView_AddSubview;      /* [28] 0x49f6ec */
    sre_u64 GUIView_RemoveFromSV;    /* [29] 0x49fa60 */
    
    /* GUIWindow (4 entries) */
    sre_u64 GUIWindow_mainWindow;    /* [30] 0x4a21ec */
    sre_u64 GUIWindow_PresentModal;  /* [31] 0x4a31b4 */
    sre_u64 GUIWindow_DismissModal;  /* [32] 0x4a3540 */
    sre_u64 GUIWindow_Dismiss;       /* [33] 0x4a3590 */
    
    /* GUIAlertView (3 entries) */
    sre_u64 GUIAlertView_SetTitle;   /* [34] 0x490628 */
    sre_u64 GUIAlertView_SetMessage; /* [35] 0x490850 */
    sre_u64 GUIAlertView_AddButton;  /* [36] 0x490bf4 */
    
    /* GameInterfaceBuilder (7 entries) */
    sre_u64 GIB_NormalLabel;         /* [37] 0x33ea24 */
    sre_u64 GIB_SmallLabel;          /* [38] 0x33ee2c */
    sre_u64 GIB_FramedButton;        /* [39] 0x33ff98 */
    sre_u64 GIB_MainMenuButton;      /* [40] 0x340504 */
    sre_u64 GIB_AlertView;           /* [41] 0x3408e0 */
    sre_u64 GIB_Slider;              /* [42] 0x341108 */
    sre_u64 GIB_Switch;              /* [43] 0x34188c */
    
    /* TextureLibrary (2 entries) */
    sre_u64 TexLib_shared;           /* [44] 0x4cc308 */
    sre_u64 TexLib_ForName;          /* [45] 0x4cc494 */
    
    /* FontLibrary (3 entries) */
    sre_u64 FontLib_shared;          /* [46] 0x4c33b0 */
    sre_u64 FontLib_DefaultFont;     /* [47] 0x4c4364 */
    sre_u64 FontLib_SmallFont;       /* [48] 0x4c44a8 */
} SreGuiAddrs;

/* Total: 49 entries × 8 bytes = 392 bytes */

/* ========== GUI Subsystem API ========== */

void sre_init_gui(SreGuiAddrs* addrs);

/* Overlay drawing */
void sre_gui_begin_overlay(void* ctx, float screenW, float screenH);
void sre_gui_end_overlay(void* ctx);
void sre_gui_fill_rect(void* ctx, float x, float y, float w, float h,
                        int r, int g, int b, int a);
void sre_gui_draw_rect(void* ctx, float x, float y, float w, float h,
                        int r, int g, int b, int a);
void sre_gui_draw_sprite(void* ctx, void* sprite, float x, float y);

/* CppString helper — constructs a stack-local SreString from a C string.
 * Caller MUST call sre_gui_free_string() when done. */
void sre_gui_make_string(SreString* out, const char* text);
void sre_gui_free_string(SreString* str);

/* Texture access */
void* sre_gui_get_texture(const char* name);

/* GameInterfaceBuilder wrappers (return shared_ptr via out param) */
void sre_gui_create_label(SreSharedPtr* out, const char* text, 
                           int r, int g, int b, int a);
void sre_gui_create_button(SreSharedPtr* out, const char* title);
void sre_gui_create_framed_button(SreSharedPtr* out, const char* title);

/* GUIView manipulation */
void sre_gui_set_frame(void* view, float x, float y, float w, float h);
void sre_gui_add_subview(void* parent, SreSharedPtr* child);
void sre_gui_remove_from_superview(void* view);

/* GUIWindow access */
void* sre_gui_main_window(void);
void sre_gui_present_modal(SreSharedPtr* view, int animated);
void sre_gui_dismiss_modal(void* view);

/* State */
extern int   g_sre_gui_initialized;
extern int   g_sre_gui_overlay_enabled;
extern float g_sre_gui_screen_w;
extern float g_sre_gui_screen_h;

#endif /* SRE_GUI_H */
