/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Boundary between the C machine layer and the C++ FLTK user interface.
 * The UI never includes hw/zeal.h (its <stdatomic.h> is not portable C++);
 * everything machine-related crosses this header as plain C data. All
 * functions run on the single UI/emulation thread. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "debugger/debugger_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FZ_WIN_NAME         "Zeal 8-bit Computer"
#define FZ_SCREEN_WIDTH     640
#define FZ_SCREEN_HEIGHT    480

/* ------------------------------------------------------------------ */
/*  Implemented by the C machine layer, used by the UI                 */
/* ------------------------------------------------------------------ */

/* Machine control */
void fz_reset(void);
void fz_quit(void);

/* Debugger: fz_debugger() is NULL when the debugger is compiled out. */
dbg_t* fz_debugger(void);
bool   fz_debugger_enabled(void);
void   fz_debugger_set_enabled(bool enabled);
bool   fz_paused(void);

/**
 * While running, the CPU state sampled at a random point of the last ~100 ms
 * (the UI itself only runs between frames). Returns false if no sample yet.
 * Any output pointer may be NULL.
 */
bool fz_live_sample(regs_t* regs, uint8_t* stack, int stack_len, uint8_t* hl, int hl_len);

/* Audio volume in percent (0-100) */
int  fz_volume(void);
void fz_volume_set(int percent);
bool fz_muted(void);
void fz_muted_set(bool muted);

/* Video */
bool fz_scanline_available(void);
bool fz_scanline(void);
void fz_scanline_set(bool enabled);

typedef enum {
    FZ_VIDEO_TEXT,
    FZ_VIDEO_GFX,
    FZ_VIDEO_BITMAP,
} fz_video_mode_t;
fz_video_mode_t fz_video_mode(void);

/* Keyboard: logical zeal_key_t codes (hw/keys.h) */
void fz_key_event(int zeal_key, bool down);
void fz_keys_release_all(void);
/** Release every held key except modifiers and lock keys. */
void fz_keys_release_nonmodifiers(void);

/* SNES mouse fed from the screen widget */
void fz_mouse_motion(float dx, float dy);
void fz_mouse_buttons(bool left, bool right);
void fz_mouse_hover(bool inside);
void fz_mouse_wheel(float steps);
void fz_mouse_capture_toggle(void);
bool fz_mouse_captured(void);

/* SNES adapter ports: port is 0/1, or -1 when detached */
int         fz_snes_gamepad_count(void);
bool        fz_snes_gamepad_available(int index);
const char* fz_snes_gamepad_name(int index);
int         fz_snes_gamepad_port(int index);
void        fz_snes_gamepad_set_port(int index, int port);
int         fz_snes_mouse_port(void);
void        fz_snes_mouse_set_port(int port);
void        fz_snes_mouse_reset_scale(void);

/* Semihost performance counters */
typedef struct {
    bool running;
    bool break_on_update;
    uint64_t min_us;
    uint64_t avg_us;
    uint64_t max_us;
    uint64_t total_us;
} fz_counter_t;
int  fz_semihost_counter_count(void);
bool fz_semihost_counter(int index, fz_counter_t* out);
void fz_semihost_toggle_break(int index);

/* VRAM debug views (dbg_vram_t), rendered on demand as packed 8-bit RGB. */
typedef struct {
    const uint8_t* pixels;
    int width;
    int height;
    /* Size of one grid cell, excluding the 1px grid line */
    int cell_width;
    int cell_height;
    int columns;
} fz_image_t;
bool fz_vram_render(dbg_vram_t view, fz_image_t* out);

/* ------------------------------------------------------------------ */
/*  Implemented by the C++ UI, used by the C machine layer             */
/* ------------------------------------------------------------------ */

int  ui_open(bool debug);
void ui_close(void);
/** True once the user closed the main window or chose Quit. */
bool ui_closed(void);
/** Service UI events for at most `seconds` (0 = poll only), then redraw. */
void ui_wait(double seconds);
/** Present a new emulator frame: RGB565 rows with a 640-pixel stride. */
void ui_present(const uint16_t* pixels, int width, int height);
/** Switch between the plain and the debugger layout. */
void ui_debug_mode(bool enabled);
/** Refresh the debugger panels (throttled internally while running). */
void ui_debug_refresh(bool force);
/** True when keyboard input should reach the emulated machine. */
bool ui_screen_focused(void);
/** Emulated pixels per host pixel of the displayed screen. */
void ui_screen_scale(float* sx, float* sy);
/** Hide/confine the host pointer for SNES mouse capture. */
void ui_pointer_lock(bool locked);
/** Rebuild menus listing host gamepads after a hot-plug. */
void ui_gamepads_changed(void);

#ifdef __cplusplus
}
#endif
