/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file display.h
 * @brief Platform seam: the services the emulator needs from the machine it runs on.
 *
 * This is a compile-time seam. Each backend implements exactly these functions and
 * only one of them is linked into a given binary, so the emulator core never includes
 * a windowing library. The desktop build links platform/display/fltk.cpp; the web build
 * links platform/display/raylib.c.
 *
 * The same backend also implements the keyboard and pointer seam, which lives in
 * include/platform/input.h.
 *
 * The one design decision worth stating: pixels cross this seam as a plain CPU buffer,
 * never as a GPU texture. The video blitter already renders into memory (see
 * ZVB_BLITTER_SOFTWARE), so a backend only has to put that buffer on screen, and the
 * debugger reads it back without a GPU round trip.
 */
#ifndef PLATFORM_DISPLAY_H
#define PLATFORM_DISPLAY_H

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
} display_color_t;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} display_rect_t;

/**
 * @brief Pixel layout of a buffer handed to display_present().
 */
typedef enum {
    /* One uint16_t per pixel, 0RRRRRGGGGGGBBBBB, as the software blitter produces. */
    DISPLAY_RGB565 = 0,
    /* One uint32_t per pixel, R in the low byte on little-endian hosts. */
    DISPLAY_RGBA8888,
} display_format_t;

/**
 * @brief Startup configuration for the window.
 */
typedef struct {
    int width;
    int height;
    const char *title;
    bool resizable;
} display_config_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Create the window and get ready to run. Audio has its own seam.
 * @return true on success
 */
bool display_init(const display_config_t *config);

/**
 * @brief Release every platform resource. Safe to call more than once.
 */
void display_shutdown(void);

/**
 * @brief True once the user asked to close the window.
 */
bool display_should_close(void);

/**
 * @brief Run one iteration of the backend's event loop.
 *
 * Only meaningful between frames
 */
void display_poll(void);

/**
 * @brief Hand the main loop to the backend, calling @p tick once per iteration.
 *
 * The backend owns the loop: FLTK drives it from its idle callback and returns when the
 * last window closes, while Raylib keeps its polling loop. @p tick runs one emulator
 * frame and returns false when the emulator wants to stop, at which point this
 * function returns.
 */
void display_run(bool (*tick)(void *user), void *user);

/* ------------------------------------------------------------------ */
/* Window                                                              */
/* ------------------------------------------------------------------ */

int display_width(void);
int display_height(void);
void display_resize(int width, int height);

/**
 * @brief Show or hide the window. A hidden window keeps running its loop, so the
 *        emulator can carry on while only the debugger is on screen.
 */
void display_show(bool visible);

void display_focus(void);
bool display_focused(void);

/**
 * @brief Frames per second the backend should pace the loop to. 0 leaves it uncapped,
 *        which is what the debugger wants once it owns the pacing.
 */
void display_frame_rate(int fps);

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */

void display_frame_begin(void);
void display_frame_end(void);
void display_clear(display_color_t color);

/**
 * @brief Put a CPU pixel buffer on screen.
 *
 * @param pixels  Buffer holding width * height pixels in @p format. It must stay valid
 *                until the frame ends.
 * @param pitch   Bytes per row.
 * @param dest    Where to draw it, in window pixels. The caller owns placement, so it
 *                can line other overlays up with the frame.
 *
 * Used by the frame-at-a-time path; the scanline path calls display_scanline() instead.
 */
void display_present(const void *pixels, int width, int height, int pitch,
                       display_format_t format, display_rect_t dest);

/**
 * @brief Hand one finished scanline to the display, top to bottom.
 *
 * @param y       Output line index, 0 through the display height - 1.
 * @param pixels  @p width pixels in @p format, valid only for the duration of the call.
 * @param width   Pixel count, always the full active width of the video mode.
 * @param pitch   Bytes per row.
 *
 * Called by the software blitter as the emulated raster reaches each line, so a line's
 * buffer is owned by the caller and may be reused immediately. What the driver does
 * with it is its own business: staging every line and uploading once per frame is what
 * a GPU backend must do, while a backend driving a real scanline display can forward
 * each call straight to its hardware and never keep a frame buffer.
 */
void display_scanline(int y, const void *pixels, int width, int pitch,
                      display_format_t format);

/**
 * @brief Draw a short string in the window, for on-screen notifications and the FPS
 *        counter. Coordinates are in window pixels.
 */
void display_text(int x, int y, const char *text, int size, display_color_t color);
int display_text_width(const char *text, int size);

/**
 * @brief Draw the backend's own frames-per-second readout. No-op when the backend
 *        does not measure one.
 */
void display_fps(int x, int y);

/* ------------------------------------------------------------------ */
/**
 * @brief Rasterize an 8x16 alpha atlas for @p codepoints.
 *
 * The debugger draws CP437 text from this atlas, so the backend owns the rasterizer:
 * FLTK uses FreeType, the WebAssembly backend uses Raylib. Fonts are found by the
 * backend, which is why no path is passed in.
 *
 * @param codepoints Unicode value for each of the 256 atlas cells.
 * @param alpha      Receives 256 * 8 * 16 bytes, row-major, 0 meaning transparent.
 * @return true when the font was found and every cell written.
 */
bool display_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16]);

/* Time                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Seconds since the platform layer started, with the same origin for every call.
 */
double display_time(void);

/**
 * @brief Sleep for the given number of seconds.
 */
void display_wait(double seconds);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_DISPLAY_H */
