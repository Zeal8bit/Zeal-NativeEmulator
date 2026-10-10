/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file raylib.c
 * @brief Raylib implementation of the platform seam.
 *
 * Used by the WebAssembly build, which has no other portable window backend. The
 * desktop build links platform/display/fltk.cpp instead; only one of the two is ever
 * compiled into a binary.
 */
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "utils/paths.h"
#include "platform/input.h"
#include "platform/display.h"

/* Raylib key codes are not dense, so the translation is a table rather than arithmetic. */
static const int raylib_keys[INPUT_KEY_COUNT] = {
    [INPUT_KEY_NONE] = 0,
    [INPUT_KEY_ESCAPE] = KEY_ESCAPE,
    [INPUT_KEY_ENTER] = KEY_ENTER,
    [INPUT_KEY_TAB] = KEY_TAB,
    [INPUT_KEY_BACKSPACE] = KEY_BACKSPACE,
    [INPUT_KEY_INSERT] = KEY_INSERT,
    [INPUT_KEY_DELETE] = KEY_DELETE,
    [INPUT_KEY_RIGHT] = KEY_RIGHT,
    [INPUT_KEY_LEFT] = KEY_LEFT,
    [INPUT_KEY_DOWN] = KEY_DOWN,
    [INPUT_KEY_UP] = KEY_UP,
    [INPUT_KEY_PAGE_UP] = KEY_PAGE_UP,
    [INPUT_KEY_PAGE_DOWN] = KEY_PAGE_DOWN,
    [INPUT_KEY_HOME] = KEY_HOME,
    [INPUT_KEY_END] = KEY_END,
    [INPUT_KEY_CAPS_LOCK] = KEY_CAPS_LOCK,
    [INPUT_KEY_SCROLL_LOCK] = KEY_SCROLL_LOCK,
    [INPUT_KEY_NUM_LOCK] = KEY_NUM_LOCK,
    [INPUT_KEY_PRINT_SCREEN] = KEY_PRINT_SCREEN,
    [INPUT_KEY_PAUSE] = KEY_PAUSE,
    [INPUT_KEY_F1] = KEY_F1,
    [INPUT_KEY_F2] = KEY_F2,
    [INPUT_KEY_F3] = KEY_F3,
    [INPUT_KEY_F4] = KEY_F4,
    [INPUT_KEY_F5] = KEY_F5,
    [INPUT_KEY_F6] = KEY_F6,
    [INPUT_KEY_F7] = KEY_F7,
    [INPUT_KEY_F8] = KEY_F8,
    [INPUT_KEY_F9] = KEY_F9,
    [INPUT_KEY_F10] = KEY_F10,
    [INPUT_KEY_F11] = KEY_F11,
    [INPUT_KEY_F12] = KEY_F12,
    [INPUT_KEY_LEFT_SHIFT] = KEY_LEFT_SHIFT,
    [INPUT_KEY_LEFT_CONTROL] = KEY_LEFT_CONTROL,
    [INPUT_KEY_LEFT_ALT] = KEY_LEFT_ALT,
    [INPUT_KEY_LEFT_SUPER] = KEY_LEFT_SUPER,
    [INPUT_KEY_RIGHT_SHIFT] = KEY_RIGHT_SHIFT,
    [INPUT_KEY_RIGHT_CONTROL] = KEY_RIGHT_CONTROL,
    [INPUT_KEY_RIGHT_ALT] = KEY_RIGHT_ALT,
    [INPUT_KEY_RIGHT_SUPER] = KEY_RIGHT_SUPER,
    [INPUT_KEY_KB_MENU] = KEY_KB_MENU,
    [INPUT_KEY_LEFT_BRACKET] = KEY_LEFT_BRACKET,
    [INPUT_KEY_RIGHT_BRACKET] = KEY_RIGHT_BRACKET,
    [INPUT_KEY_BACKSLASH] = KEY_BACKSLASH,
    [INPUT_KEY_SEMICOLON] = KEY_SEMICOLON,
    [INPUT_KEY_APOSTROPHE] = KEY_APOSTROPHE,
    [INPUT_KEY_MINUS] = KEY_MINUS,
    [INPUT_KEY_EQUAL] = KEY_EQUAL,
    [INPUT_KEY_GRAVE] = KEY_GRAVE,
    [INPUT_KEY_COMMA] = KEY_COMMA,
    [INPUT_KEY_PERIOD] = KEY_PERIOD,
    [INPUT_KEY_SLASH] = KEY_SLASH,
    [INPUT_KEY_SPACE] = KEY_SPACE,
    [INPUT_KEY_ZERO] = KEY_ZERO,
    [INPUT_KEY_ONE] = KEY_ONE,
    [INPUT_KEY_TWO] = KEY_TWO,
    [INPUT_KEY_THREE] = KEY_THREE,
    [INPUT_KEY_FOUR] = KEY_FOUR,
    [INPUT_KEY_FIVE] = KEY_FIVE,
    [INPUT_KEY_SIX] = KEY_SIX,
    [INPUT_KEY_SEVEN] = KEY_SEVEN,
    [INPUT_KEY_EIGHT] = KEY_EIGHT,
    [INPUT_KEY_NINE] = KEY_NINE,
    [INPUT_KEY_A] = KEY_A,
    [INPUT_KEY_B] = KEY_B,
    [INPUT_KEY_C] = KEY_C,
    [INPUT_KEY_D] = KEY_D,
    [INPUT_KEY_E] = KEY_E,
    [INPUT_KEY_F] = KEY_F,
    [INPUT_KEY_G] = KEY_G,
    [INPUT_KEY_H] = KEY_H,
    [INPUT_KEY_I] = KEY_I,
    [INPUT_KEY_J] = KEY_J,
    [INPUT_KEY_K] = KEY_K,
    [INPUT_KEY_L] = KEY_L,
    [INPUT_KEY_M] = KEY_M,
    [INPUT_KEY_N] = KEY_N,
    [INPUT_KEY_O] = KEY_O,
    [INPUT_KEY_P] = KEY_P,
    [INPUT_KEY_Q] = KEY_Q,
    [INPUT_KEY_R] = KEY_R,
    [INPUT_KEY_S] = KEY_S,
    [INPUT_KEY_T] = KEY_T,
    [INPUT_KEY_U] = KEY_U,
    [INPUT_KEY_V] = KEY_V,
    [INPUT_KEY_W] = KEY_W,
    [INPUT_KEY_X] = KEY_X,
    [INPUT_KEY_Y] = KEY_Y,
    [INPUT_KEY_Z] = KEY_Z,
    [INPUT_KEY_KP_0] = KEY_KP_0,
    [INPUT_KEY_KP_1] = KEY_KP_1,
    [INPUT_KEY_KP_2] = KEY_KP_2,
    [INPUT_KEY_KP_3] = KEY_KP_3,
    [INPUT_KEY_KP_4] = KEY_KP_4,
    [INPUT_KEY_KP_5] = KEY_KP_5,
    [INPUT_KEY_KP_6] = KEY_KP_6,
    [INPUT_KEY_KP_7] = KEY_KP_7,
    [INPUT_KEY_KP_8] = KEY_KP_8,
    [INPUT_KEY_KP_9] = KEY_KP_9,
    [INPUT_KEY_KP_DECIMAL] = KEY_KP_DECIMAL,
    [INPUT_KEY_KP_DIVIDE] = KEY_KP_DIVIDE,
    [INPUT_KEY_KP_MULTIPLY] = KEY_KP_MULTIPLY,
    [INPUT_KEY_KP_SUBTRACT] = KEY_KP_SUBTRACT,
    [INPUT_KEY_KP_ADD] = KEY_KP_ADD,
    [INPUT_KEY_KP_ENTER] = KEY_KP_ENTER,
    [INPUT_KEY_KP_EQUAL] = KEY_KP_EQUAL,
};

/* Present buffers are uploaded into one reusable texture. */
static Texture2D present_texture;
static bool present_texture_ready;
static int present_texture_width;
static int present_texture_height;

static bool raylib_init_window(const display_config_t* config)
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

bool display_init(const display_config_t* config)
{
    if (config == NULL) {
        return false;
    }
    return raylib_init_window(config);
}

void display_shutdown(void)
{
    if (present_texture_ready) {
        UnloadTexture(present_texture);
        present_texture_ready = false;
    }
    CloseWindow();
}

bool display_should_close(void)
{
    return WindowShouldClose();
}

void display_poll(void)
{
    PollInputEvents();
}

void display_run(bool (*tick)(void *user), void *user)
{
    while (!WindowShouldClose()) {
        if (!tick(user)) {
            break;
        }
    }
}

int display_width(void) { return GetScreenWidth(); }
int display_height(void) { return GetScreenHeight(); }
void display_resize(int width, int height) { SetWindowSize(width, height); }
void display_show(bool visible)
{
    if (visible) {
        ClearWindowState(FLAG_WINDOW_HIDDEN);
        SetWindowFocused();
    } else {
        SetWindowState(FLAG_WINDOW_HIDDEN);
    }
}
void display_focus(void) { SetWindowFocused(); }
bool display_focused(void) { return IsWindowFocused(); }
void display_frame_rate(int fps) { SetTargetFPS(fps); }

void display_frame_begin(void) { BeginDrawing(); }
void display_frame_end(void) { EndDrawing(); }
void display_clear(display_color_t color)
{
    ClearBackground((Color){color.r, color.g, color.b, color.a});
}

void display_present(const void* pixels, int width, int height, int pitch, display_format_t format,
                       display_rect_t dest)
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

    if (format == DISPLAY_RGB565) {
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

void display_text(int x, int y, const char* text, int size, display_color_t color)
{
    if (text == NULL) {
        return;
    }
    DrawText(text, x, y, size, (Color){color.r, color.g, color.b, color.a});
}

void display_fps(int x, int y)
{
    DrawFPS(x, y);
}

int display_text_width(const char* text, int size)
{
    return text == NULL ? 0 : MeasureText(text, size);
}

bool display_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16])
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

double display_time(void) { return GetTime(); }
void display_wait(double seconds) { WaitTime(seconds); }

int input_key_pressed(void)
{
    int key = GetKeyPressed();
    if (key == 0) {
        return INPUT_KEY_NONE;
    }
    for (int i = 1; i < INPUT_KEY_COUNT; i++) {
        if (raylib_keys[i] == key) {
            return i;
        }
    }
    return INPUT_KEY_NONE;
}

bool input_key_down(int key)
{
    return key > 0 && key < INPUT_KEY_COUNT && IsKeyDown(raylib_keys[key]);
}

bool input_key_up(int key)
{
    return key <= 0 || key >= INPUT_KEY_COUNT || IsKeyUp(raylib_keys[key]);
}

bool input_mouse_down(int button)
{
    return IsMouseButtonDown(button);
}

input_point_t input_mouse_position(void)
{
    /* Raylib tracks whole pixels, so the cast is exact. */
    const Vector2 position = GetMousePosition();
    return (input_point_t){(int)position.x, (int)position.y};
}

input_delta_t input_mouse_delta(void)
{
    /* Raylib tracks whole pixels, so the casts are exact. */
    const Vector2 delta = GetMouseDelta();
    return (input_delta_t){(int)delta.x, (int)delta.y};
}

int input_mouse_wheel(void)
{
    /* Raylib reports whole notches and is already up-positive, matching the seam. */
    return (int)GetMouseWheelMove();
}

void input_mouse_capture(bool capture)
{
    if (capture) {
        DisableCursor();
    } else {
        EnableCursor();
    }
}
