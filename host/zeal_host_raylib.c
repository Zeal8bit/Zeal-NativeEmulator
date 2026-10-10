/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_host_raylib.c
 * @brief Raylib implementation of the host seam.
 *
 * Used by the WebAssembly build, which has no other portable window backend. The
 * desktop build links host/zeal_host_fltk.cpp instead; only one of the two is ever
 * compiled into a binary.
 */
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "utils/paths.h"
#include "host/zeal_host.h"

/* Raylib key codes are not dense, so the translation is a table rather than arithmetic. */
static const int raylib_keys[ZEAL_HOST_KEY_COUNT] = {
    [ZEAL_HOST_KEY_NONE] = 0,
    [ZEAL_HOST_KEY_ESCAPE] = KEY_ESCAPE,
    [ZEAL_HOST_KEY_ENTER] = KEY_ENTER,
    [ZEAL_HOST_KEY_TAB] = KEY_TAB,
    [ZEAL_HOST_KEY_BACKSPACE] = KEY_BACKSPACE,
    [ZEAL_HOST_KEY_INSERT] = KEY_INSERT,
    [ZEAL_HOST_KEY_DELETE] = KEY_DELETE,
    [ZEAL_HOST_KEY_RIGHT] = KEY_RIGHT,
    [ZEAL_HOST_KEY_LEFT] = KEY_LEFT,
    [ZEAL_HOST_KEY_DOWN] = KEY_DOWN,
    [ZEAL_HOST_KEY_UP] = KEY_UP,
    [ZEAL_HOST_KEY_PAGE_UP] = KEY_PAGE_UP,
    [ZEAL_HOST_KEY_PAGE_DOWN] = KEY_PAGE_DOWN,
    [ZEAL_HOST_KEY_HOME] = KEY_HOME,
    [ZEAL_HOST_KEY_END] = KEY_END,
    [ZEAL_HOST_KEY_CAPS_LOCK] = KEY_CAPS_LOCK,
    [ZEAL_HOST_KEY_SCROLL_LOCK] = KEY_SCROLL_LOCK,
    [ZEAL_HOST_KEY_NUM_LOCK] = KEY_NUM_LOCK,
    [ZEAL_HOST_KEY_PRINT_SCREEN] = KEY_PRINT_SCREEN,
    [ZEAL_HOST_KEY_PAUSE] = KEY_PAUSE,
    [ZEAL_HOST_KEY_F1] = KEY_F1,
    [ZEAL_HOST_KEY_F2] = KEY_F2,
    [ZEAL_HOST_KEY_F3] = KEY_F3,
    [ZEAL_HOST_KEY_F4] = KEY_F4,
    [ZEAL_HOST_KEY_F5] = KEY_F5,
    [ZEAL_HOST_KEY_F6] = KEY_F6,
    [ZEAL_HOST_KEY_F7] = KEY_F7,
    [ZEAL_HOST_KEY_F8] = KEY_F8,
    [ZEAL_HOST_KEY_F9] = KEY_F9,
    [ZEAL_HOST_KEY_F10] = KEY_F10,
    [ZEAL_HOST_KEY_F11] = KEY_F11,
    [ZEAL_HOST_KEY_F12] = KEY_F12,
    [ZEAL_HOST_KEY_LEFT_SHIFT] = KEY_LEFT_SHIFT,
    [ZEAL_HOST_KEY_LEFT_CONTROL] = KEY_LEFT_CONTROL,
    [ZEAL_HOST_KEY_LEFT_ALT] = KEY_LEFT_ALT,
    [ZEAL_HOST_KEY_LEFT_SUPER] = KEY_LEFT_SUPER,
    [ZEAL_HOST_KEY_RIGHT_SHIFT] = KEY_RIGHT_SHIFT,
    [ZEAL_HOST_KEY_RIGHT_CONTROL] = KEY_RIGHT_CONTROL,
    [ZEAL_HOST_KEY_RIGHT_ALT] = KEY_RIGHT_ALT,
    [ZEAL_HOST_KEY_RIGHT_SUPER] = KEY_RIGHT_SUPER,
    [ZEAL_HOST_KEY_KB_MENU] = KEY_KB_MENU,
    [ZEAL_HOST_KEY_LEFT_BRACKET] = KEY_LEFT_BRACKET,
    [ZEAL_HOST_KEY_RIGHT_BRACKET] = KEY_RIGHT_BRACKET,
    [ZEAL_HOST_KEY_BACKSLASH] = KEY_BACKSLASH,
    [ZEAL_HOST_KEY_SEMICOLON] = KEY_SEMICOLON,
    [ZEAL_HOST_KEY_APOSTROPHE] = KEY_APOSTROPHE,
    [ZEAL_HOST_KEY_MINUS] = KEY_MINUS,
    [ZEAL_HOST_KEY_EQUAL] = KEY_EQUAL,
    [ZEAL_HOST_KEY_GRAVE] = KEY_GRAVE,
    [ZEAL_HOST_KEY_COMMA] = KEY_COMMA,
    [ZEAL_HOST_KEY_PERIOD] = KEY_PERIOD,
    [ZEAL_HOST_KEY_SLASH] = KEY_SLASH,
    [ZEAL_HOST_KEY_SPACE] = KEY_SPACE,
    [ZEAL_HOST_KEY_ZERO] = KEY_ZERO,
    [ZEAL_HOST_KEY_ONE] = KEY_ONE,
    [ZEAL_HOST_KEY_TWO] = KEY_TWO,
    [ZEAL_HOST_KEY_THREE] = KEY_THREE,
    [ZEAL_HOST_KEY_FOUR] = KEY_FOUR,
    [ZEAL_HOST_KEY_FIVE] = KEY_FIVE,
    [ZEAL_HOST_KEY_SIX] = KEY_SIX,
    [ZEAL_HOST_KEY_SEVEN] = KEY_SEVEN,
    [ZEAL_HOST_KEY_EIGHT] = KEY_EIGHT,
    [ZEAL_HOST_KEY_NINE] = KEY_NINE,
    [ZEAL_HOST_KEY_A] = KEY_A,
    [ZEAL_HOST_KEY_B] = KEY_B,
    [ZEAL_HOST_KEY_C] = KEY_C,
    [ZEAL_HOST_KEY_D] = KEY_D,
    [ZEAL_HOST_KEY_E] = KEY_E,
    [ZEAL_HOST_KEY_F] = KEY_F,
    [ZEAL_HOST_KEY_G] = KEY_G,
    [ZEAL_HOST_KEY_H] = KEY_H,
    [ZEAL_HOST_KEY_I] = KEY_I,
    [ZEAL_HOST_KEY_J] = KEY_J,
    [ZEAL_HOST_KEY_K] = KEY_K,
    [ZEAL_HOST_KEY_L] = KEY_L,
    [ZEAL_HOST_KEY_M] = KEY_M,
    [ZEAL_HOST_KEY_N] = KEY_N,
    [ZEAL_HOST_KEY_O] = KEY_O,
    [ZEAL_HOST_KEY_P] = KEY_P,
    [ZEAL_HOST_KEY_Q] = KEY_Q,
    [ZEAL_HOST_KEY_R] = KEY_R,
    [ZEAL_HOST_KEY_S] = KEY_S,
    [ZEAL_HOST_KEY_T] = KEY_T,
    [ZEAL_HOST_KEY_U] = KEY_U,
    [ZEAL_HOST_KEY_V] = KEY_V,
    [ZEAL_HOST_KEY_W] = KEY_W,
    [ZEAL_HOST_KEY_X] = KEY_X,
    [ZEAL_HOST_KEY_Y] = KEY_Y,
    [ZEAL_HOST_KEY_Z] = KEY_Z,
    [ZEAL_HOST_KEY_KP_0] = KEY_KP_0,
    [ZEAL_HOST_KEY_KP_1] = KEY_KP_1,
    [ZEAL_HOST_KEY_KP_2] = KEY_KP_2,
    [ZEAL_HOST_KEY_KP_3] = KEY_KP_3,
    [ZEAL_HOST_KEY_KP_4] = KEY_KP_4,
    [ZEAL_HOST_KEY_KP_5] = KEY_KP_5,
    [ZEAL_HOST_KEY_KP_6] = KEY_KP_6,
    [ZEAL_HOST_KEY_KP_7] = KEY_KP_7,
    [ZEAL_HOST_KEY_KP_8] = KEY_KP_8,
    [ZEAL_HOST_KEY_KP_9] = KEY_KP_9,
    [ZEAL_HOST_KEY_KP_DECIMAL] = KEY_KP_DECIMAL,
    [ZEAL_HOST_KEY_KP_DIVIDE] = KEY_KP_DIVIDE,
    [ZEAL_HOST_KEY_KP_MULTIPLY] = KEY_KP_MULTIPLY,
    [ZEAL_HOST_KEY_KP_SUBTRACT] = KEY_KP_SUBTRACT,
    [ZEAL_HOST_KEY_KP_ADD] = KEY_KP_ADD,
    [ZEAL_HOST_KEY_KP_ENTER] = KEY_KP_ENTER,
    [ZEAL_HOST_KEY_KP_EQUAL] = KEY_KP_EQUAL,
};

/* Present buffers are uploaded into one reusable texture. */
static Texture2D present_texture;
static bool present_texture_ready;
static int present_texture_width;
static int present_texture_height;

static bool raylib_init_window(const zeal_host_config_t* config)
{
    SetTraceLogLevel(LOG_WARNING);
#ifndef PLATFORM_WEB
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(config->width, config->height, config->title);
    SetExitKey(KEY_NULL);
#ifndef PLATFORM_WEB
    SetWindowFocused(); /* force focus on the window to capture keypresses */
#endif
    SetTargetFPS(60);

    /* Force rendering the window, to allow Raylib peripherals to attach (ie; gamepads) */
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
    return true;
}

bool zeal_host_init(const zeal_host_config_t* config)
{
    if (config == NULL) {
        return false;
    }
    return raylib_init_window(config);
}

void zeal_host_shutdown(void)
{
    if (present_texture_ready) {
        UnloadTexture(present_texture);
        present_texture_ready = false;
    }
    CloseWindow();
}

bool zeal_host_should_close(void)
{
    return WindowShouldClose();
}

void zeal_host_poll(void)
{
    PollInputEvents();
}

void zeal_host_run(bool (*tick)(void *user), void *user)
{
    while (!WindowShouldClose()) {
        if (!tick(user)) {
            break;
        }
    }
}

int zeal_host_width(void) { return GetScreenWidth(); }
int zeal_host_height(void) { return GetScreenHeight(); }
void zeal_host_resize(int width, int height) { SetWindowSize(width, height); }
void zeal_host_show(bool visible)
{
    if (visible) {
        ClearWindowState(FLAG_WINDOW_HIDDEN);
        SetWindowFocused();
    } else {
        SetWindowState(FLAG_WINDOW_HIDDEN);
    }
}
void zeal_host_focus(void) { SetWindowFocused(); }
bool zeal_host_focused(void) { return IsWindowFocused(); }
void zeal_host_frame_rate(int fps) { SetTargetFPS(fps); }

void zeal_host_frame_begin(void) { BeginDrawing(); }
void zeal_host_frame_end(void) { EndDrawing(); }
void zeal_host_clear(zeal_host_color_t color)
{
    ClearBackground((Color){color.r, color.g, color.b, color.a});
}

void zeal_host_present(const void* pixels, int width, int height, int pitch, zeal_host_format_t format,
                       zeal_host_rect_t dest)
{
    if (pixels == NULL || width <= 0 || height <= 0) {
        return;
    }
    (void)pitch;
    if (!present_texture_ready || present_texture_width != width || present_texture_height != height) {
        if (present_texture_ready) {
            UnloadTexture(present_texture);
        }
        Image image = GenImageColor(width, height, BLANK);
        present_texture = LoadTextureFromImage(image);
        UnloadImage(image);
        present_texture_ready = true;
        present_texture_width = width;
        present_texture_height = height;
        SetTextureFilter(present_texture, TEXTURE_FILTER_POINT);
    }

    if (format == ZEAL_HOST_RGB565) {
        /* Raylib has no RGB565 upload, so expand into an RGBA scratch buffer. */
        static unsigned char* scratch;
        static size_t scratch_size;
        size_t needed = (size_t)width * height * 4;
        if (scratch_size < needed) {
            unsigned char* grown = realloc(scratch, needed);
            if (grown == NULL) {
                return;
            }
            scratch = grown;
            scratch_size = needed;
        }
        const uint16_t* src = pixels;
        for (int i = 0; i < width * height; i++) {
            uint16_t p = src[i];
            scratch[i * 4 + 0] = (unsigned char)(((p >> 11) & 0x1F) * 255 / 31);
            scratch[i * 4 + 1] = (unsigned char)(((p >> 5) & 0x3F) * 255 / 63);
            scratch[i * 4 + 2] = (unsigned char)((p & 0x1F) * 255 / 31);
            scratch[i * 4 + 3] = 255;
        }
        UpdateTexture(present_texture, scratch);
    } else {
        UpdateTexture(present_texture, pixels);
    }

    DrawTexturePro(present_texture, (Rectangle){0, 0, (float)width, (float)height},
                   (Rectangle){dest.x, dest.y, dest.width, dest.height}, (Vector2){0, 0}, 0, WHITE);
}

void zeal_host_text(int x, int y, const char* text, int size, zeal_host_color_t color)
{
    if (text == NULL) {
        return;
    }
    DrawText(text, x, y, size, (Color){color.r, color.g, color.b, color.a});
}

void zeal_host_fps(int x, int y)
{
    DrawFPS(x, y);
}

int zeal_host_text_width(const char* text, int size)
{
    return text == NULL ? 0 : MeasureText(text, size);
}

bool zeal_host_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16])
{
    char path[PATH_MAX];
    get_install_dir_file(path, "assets/fonts/BigBlue_Terminal_437TT.TTF");
    if (!FileExists(path)) {
        snprintf(path, sizeof(path), "assets/fonts/BigBlue_Terminal_437TT.TTF");
    }
#ifdef ZEAL_ASSETS_DIR
    if (!FileExists(path)) {
        snprintf(path, sizeof(path), "%s/fonts/BigBlue_Terminal_437TT.TTF", ZEAL_ASSETS_DIR);
    }
#endif
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (data == NULL) {
        return false;
    }
    int codes[256];
    for (unsigned i = 0; i < 256; i++) {
        codes[i] = (int)codepoints[i];
    }
    GlyphInfo *glyphs = LoadFontData(data, size, 16, codes, 256, FONT_BITMAP);
    UnloadFileData(data);
    if (glyphs == NULL) {
        return false;
    }
    memset(alpha, 0, 256 * 8 * 16);
    for (unsigned i = 0; i < 256; i++) {
        Image *image = &glyphs[i].image;
        bool mask = image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        for (int y = 0; y < image->height; y++) {
            for (int x = 0; x < image->width; x++) {
                int dx = x + glyphs[i].offsetX, dy = y + glyphs[i].offsetY;
                if (dx >= 0 && dx < 8 && dy >= 0 && dy < 16) {
                    alpha[i * 128 + dy * 8 + dx] =
                        ((uint8_t *)image->data)[(y * image->width + x) * 4 + (mask ? 0 : 3)];
                }
            }
        }
    }
    UnloadFontData(glyphs, 256);
    return true;
}

double zeal_host_time(void) { return GetTime(); }
void zeal_host_wait(double seconds) { WaitTime(seconds); }

int zeal_host_key_pressed(void)
{
    int key = GetKeyPressed();
    if (key == 0) {
        return ZEAL_HOST_KEY_NONE;
    }
    for (int i = 1; i < ZEAL_HOST_KEY_COUNT; i++) {
        if (raylib_keys[i] == key) {
            return i;
        }
    }
    return ZEAL_HOST_KEY_NONE;
}

bool zeal_host_key_down(int key)
{
    return key > 0 && key < ZEAL_HOST_KEY_COUNT && IsKeyDown(raylib_keys[key]);
}

bool zeal_host_key_up(int key)
{
    return key <= 0 || key >= ZEAL_HOST_KEY_COUNT || IsKeyUp(raylib_keys[key]);
}

bool zeal_host_mouse_down(int button)
{
    return IsMouseButtonDown(button);
}

zeal_host_vec2_t zeal_host_mouse_position(void)
{
    Vector2 position = GetMousePosition();
    return (zeal_host_vec2_t){position.x, position.y};
}

void zeal_host_mouse_delta(float* dx, float* dy)
{
    Vector2 delta = GetMouseDelta();
    if (dx) {
        *dx = delta.x;
    }
    if (dy) {
        *dy = delta.y;
    }
}

float zeal_host_mouse_wheel(void)
{
    return GetMouseWheelMove();
}

void zeal_host_mouse_capture(bool capture)
{
    if (capture) {
        DisableCursor();
    } else {
        EnableCursor();
    }
}
