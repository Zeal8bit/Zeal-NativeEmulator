/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_host.h
 * @brief Host seam: the services the emulator needs from the machine it runs on.
 *
 * This is a compile-time seam. Each backend implements exactly these functions and
 * only one of them is linked into a given binary, so the emulator core never includes
 * a windowing library. The desktop build links host/zeal_host_fltk.cpp; the web build
 * links host/zeal_host_raylib.c.
 *
 * The one design decision worth stating: pixels cross this seam as a plain CPU buffer,
 * never as a GPU texture. The video blitter already renders into memory (see
 * ZVB_BLITTER_SOFTWARE), so a backend only has to put that buffer on screen, and the
 * debugger reads it back without a GPU round trip.
 */
#ifndef ZEAL_HOST_H
#define ZEAL_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Shared value types, independent of any backend                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} zeal_host_color_t;

typedef struct {
    float x;
    float y;
} zeal_host_vec2_t;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} zeal_host_rect_t;

/**
 * @brief Pixel layout of a buffer handed to zeal_host_present().
 */
typedef enum {
    /* One uint16_t per pixel, 0RRRRRGGGGGGBBBBB, as the software blitter produces. */
    ZEAL_HOST_RGB565 = 0,
    /* One uint32_t per pixel, R in the low byte on little-endian hosts. */
    ZEAL_HOST_RGBA8888,
} zeal_host_format_t;

/**
 * @brief Startup configuration for the window.
 */
typedef struct {
    int width;
    int height;
    const char *title;
    bool resizable;
} zeal_host_config_t;

/**
 * @brief Keys the emulator and the debugger act on.
 *
 * Backends translate their native key codes into these, so no key constant from a
 * windowing library reaches the core. ZEAL_HOST_KEY_NONE means "no key".
 */
typedef enum {
    ZEAL_HOST_KEY_NONE = 0,
    ZEAL_HOST_KEY_ESCAPE = 1,
    ZEAL_HOST_KEY_ENTER,
    ZEAL_HOST_KEY_TAB,
    ZEAL_HOST_KEY_BACKSPACE,
    ZEAL_HOST_KEY_INSERT,
    ZEAL_HOST_KEY_DELETE,
    ZEAL_HOST_KEY_RIGHT,
    ZEAL_HOST_KEY_LEFT,
    ZEAL_HOST_KEY_DOWN,
    ZEAL_HOST_KEY_UP,
    ZEAL_HOST_KEY_PAGE_UP,
    ZEAL_HOST_KEY_PAGE_DOWN,
    ZEAL_HOST_KEY_HOME,
    ZEAL_HOST_KEY_END,
    ZEAL_HOST_KEY_CAPS_LOCK,
    ZEAL_HOST_KEY_SCROLL_LOCK,
    ZEAL_HOST_KEY_NUM_LOCK,
    ZEAL_HOST_KEY_PRINT_SCREEN,
    ZEAL_HOST_KEY_PAUSE,
    ZEAL_HOST_KEY_F1,
    ZEAL_HOST_KEY_F2,
    ZEAL_HOST_KEY_F3,
    ZEAL_HOST_KEY_F4,
    ZEAL_HOST_KEY_F5,
    ZEAL_HOST_KEY_F6,
    ZEAL_HOST_KEY_F7,
    ZEAL_HOST_KEY_F8,
    ZEAL_HOST_KEY_F9,
    ZEAL_HOST_KEY_F10,
    ZEAL_HOST_KEY_F11,
    ZEAL_HOST_KEY_F12,
    ZEAL_HOST_KEY_LEFT_SHIFT,
    ZEAL_HOST_KEY_LEFT_CONTROL,
    ZEAL_HOST_KEY_LEFT_ALT,
    ZEAL_HOST_KEY_LEFT_SUPER,
    ZEAL_HOST_KEY_RIGHT_SHIFT,
    ZEAL_HOST_KEY_RIGHT_CONTROL,
    ZEAL_HOST_KEY_RIGHT_ALT,
    ZEAL_HOST_KEY_RIGHT_SUPER,
    ZEAL_HOST_KEY_KB_MENU,
    ZEAL_HOST_KEY_LEFT_BRACKET,
    ZEAL_HOST_KEY_RIGHT_BRACKET,
    ZEAL_HOST_KEY_BACKSLASH,
    ZEAL_HOST_KEY_SEMICOLON,
    ZEAL_HOST_KEY_APOSTROPHE,
    ZEAL_HOST_KEY_MINUS,
    ZEAL_HOST_KEY_EQUAL,
    ZEAL_HOST_KEY_GRAVE,
    ZEAL_HOST_KEY_COMMA,
    ZEAL_HOST_KEY_PERIOD,
    ZEAL_HOST_KEY_SLASH,
    ZEAL_HOST_KEY_SPACE,
    ZEAL_HOST_KEY_ZERO,
    ZEAL_HOST_KEY_ONE,
    ZEAL_HOST_KEY_TWO,
    ZEAL_HOST_KEY_THREE,
    ZEAL_HOST_KEY_FOUR,
    ZEAL_HOST_KEY_FIVE,
    ZEAL_HOST_KEY_SIX,
    ZEAL_HOST_KEY_SEVEN,
    ZEAL_HOST_KEY_EIGHT,
    ZEAL_HOST_KEY_NINE,
    ZEAL_HOST_KEY_A,
    ZEAL_HOST_KEY_B,
    ZEAL_HOST_KEY_C,
    ZEAL_HOST_KEY_D,
    ZEAL_HOST_KEY_E,
    ZEAL_HOST_KEY_F,
    ZEAL_HOST_KEY_G,
    ZEAL_HOST_KEY_H,
    ZEAL_HOST_KEY_I,
    ZEAL_HOST_KEY_J,
    ZEAL_HOST_KEY_K,
    ZEAL_HOST_KEY_L,
    ZEAL_HOST_KEY_M,
    ZEAL_HOST_KEY_N,
    ZEAL_HOST_KEY_O,
    ZEAL_HOST_KEY_P,
    ZEAL_HOST_KEY_Q,
    ZEAL_HOST_KEY_R,
    ZEAL_HOST_KEY_S,
    ZEAL_HOST_KEY_T,
    ZEAL_HOST_KEY_U,
    ZEAL_HOST_KEY_V,
    ZEAL_HOST_KEY_W,
    ZEAL_HOST_KEY_X,
    ZEAL_HOST_KEY_Y,
    ZEAL_HOST_KEY_Z,
    ZEAL_HOST_KEY_KP_0,
    ZEAL_HOST_KEY_KP_1,
    ZEAL_HOST_KEY_KP_2,
    ZEAL_HOST_KEY_KP_3,
    ZEAL_HOST_KEY_KP_4,
    ZEAL_HOST_KEY_KP_5,
    ZEAL_HOST_KEY_KP_6,
    ZEAL_HOST_KEY_KP_7,
    ZEAL_HOST_KEY_KP_8,
    ZEAL_HOST_KEY_KP_9,
    ZEAL_HOST_KEY_KP_DECIMAL,
    ZEAL_HOST_KEY_KP_DIVIDE,
    ZEAL_HOST_KEY_KP_MULTIPLY,
    ZEAL_HOST_KEY_KP_SUBTRACT,
    ZEAL_HOST_KEY_KP_ADD,
    ZEAL_HOST_KEY_KP_ENTER,
    ZEAL_HOST_KEY_KP_EQUAL,
    ZEAL_HOST_KEY_COUNT
} zeal_host_key_t;

typedef enum {
    ZEAL_HOST_MOUSE_LEFT = 0,
    ZEAL_HOST_MOUSE_RIGHT,
    ZEAL_HOST_MOUSE_MIDDLE,
    ZEAL_HOST_MOUSE_BUTTON_COUNT
} zeal_host_mouse_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Create the window and get ready to run. Audio has its own seam.
 * @return true on success
 */
bool zeal_host_init(const zeal_host_config_t *config);

/**
 * @brief Release every host resource. Safe to call more than once.
 */
void zeal_host_shutdown(void);

/**
 * @brief True once the user asked to close the window.
 */
bool zeal_host_should_close(void);

/**
 * @brief Run one iteration of the backend's event loop.
 *
 * Only meaningful between frames; the loop itself is zeal_host_run().
 */
void zeal_host_poll(void);

/**
 * @brief Hand the main loop to the host, calling @p tick once per iteration.
 *
 * The host owns the loop: FLTK drives it from its idle callback and returns when the
 * last window closes, while Raylib keeps its polling loop. @p tick runs one emulator
 * frame and returns false when the emulator wants to stop, at which point this
 * function returns.
 */
void zeal_host_run(bool (*tick)(void *user), void *user);

/* ------------------------------------------------------------------ */
/* Window                                                              */
/* ------------------------------------------------------------------ */

int zeal_host_width(void);
int zeal_host_height(void);
void zeal_host_resize(int width, int height);

/**
 * @brief Show or hide the window. A hidden window keeps running its loop, so the
 *        emulator can carry on while only the debugger is on screen.
 */
void zeal_host_show(bool visible);

void zeal_host_focus(void);
bool zeal_host_focused(void);

/**
 * @brief Frames per second the backend should pace the loop to. 0 leaves it uncapped,
 *        which is what the debugger wants once it owns the pacing.
 */
void zeal_host_frame_rate(int fps);

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */

void zeal_host_frame_begin(void);
void zeal_host_frame_end(void);
void zeal_host_clear(zeal_host_color_t color);

/**
 * @brief Put a CPU pixel buffer on screen.
 *
 * @param pixels  Buffer holding width * height pixels in @p format. It must stay valid
 *                until the frame ends.
 * @param pitch   Bytes per row.
 * @param dest    Where to draw it, in window pixels. The caller owns placement, so it
 *                can line other overlays up with the frame.
 */
void zeal_host_present(const void *pixels, int width, int height, int pitch,
                       zeal_host_format_t format, zeal_host_rect_t dest);

/**
 * @brief Draw a short string in the window, for on-screen notifications and the FPS
 *        counter. Coordinates are in window pixels.
 */
void zeal_host_text(int x, int y, const char *text, int size, zeal_host_color_t color);
int zeal_host_text_width(const char *text, int size);

/**
 * @brief Draw the backend's own frames-per-second readout. No-op when the backend
 *        does not measure one.
 */
void zeal_host_fps(int x, int y);

/* ------------------------------------------------------------------ */
/**
 * @brief Rasterize an 8x16 alpha atlas for @p codepoints.
 *
 * The debugger draws CP437 text from this atlas, so the host owns the rasterizer:
 * FLTK uses FreeType, the WebAssembly host uses Raylib. Fonts are found by the
 * backend, which is why no path is passed in.
 *
 * @param codepoints Unicode value for each of the 256 atlas cells.
 * @param alpha      Receives 256 * 8 * 16 bytes, row-major, 0 meaning transparent.
 * @return true when the font was found and every cell written.
 */
bool zeal_host_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16]);

/* Time                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Seconds since the host started, with the same origin for every call.
 */
double zeal_host_time(void);

/**
 * @brief Sleep for the given number of seconds.
 */
void zeal_host_wait(double seconds);

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Pop the oldest key pressed since the last call, or ZEAL_HOST_KEY_NONE.
 */
int zeal_host_key_pressed(void);

bool zeal_host_key_down(int key);
bool zeal_host_key_up(int key);

bool zeal_host_mouse_down(int button);

/**
 * @brief Pointer position within the window, in pixels.
 */
zeal_host_vec2_t zeal_host_mouse_position(void);

/**
 * @brief Mouse movement since the previous frame, in pixels.
 */
void zeal_host_mouse_delta(float *dx, float *dy);

/**
 * @brief Wheel movement since the previous frame.
 */
float zeal_host_mouse_wheel(void);

/**
 * @brief Grab or release the pointer and hide or show its cursor.
 */
void zeal_host_mouse_capture(bool capture);

#ifdef __cplusplus
}
#endif

#endif /* ZEAL_HOST_H */
