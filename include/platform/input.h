/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file input.h
 * @brief Input seam: keyboard and pointer events the emulator acts on.
 *
 * This is the second half of the display seam, kept in its own header because it is
 * the part the emulator and debugger consume directly. Both are provided by the same
 * window backend and there is exactly one of it per binary, so platform/display/ holds
 * the implementation of these functions: the backend owns the event loop, and the key
 * and pointer state it maintains while handling events is what these functions report.
 * Text and frame drawing live in include/platform/display.h.
 *
 * Backends translate their native key codes into INPUT_KEY_* values, so no key
 * constant from a windowing library reaches the emulator core.
 */
#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A pointer position in window pixels.
 *
 * Integer because every backend's events carry integer pixels: FLTK's event_x() and
 * event_y() are int, and Raylib's Vector2 holds values that came from integer OS
 * coordinates. There is no sub-pixel information to preserve.
 */
typedef struct {
    int x;
    int y;
} input_point_t;

/**
 * @brief Pointer displacement accumulated over one frame, in pixels.
 *
 * Also integer, and for the same reason: a displacement is the difference between two
 * integer positions. Backends accumulate it between frames and reset it when the
 * emulator reads it, so a frame's worth of small movements is summed rather than lost.
 */
typedef struct {
    int dx;
    int dy;
} input_delta_t;

/**
 * @brief Keys the emulator and the debugger act on.
 *
 * INPUT_KEY_NONE means "no key".
 */
typedef enum {
    INPUT_KEY_NONE = 0,
    INPUT_KEY_ESCAPE = 1,
    INPUT_KEY_ENTER,
    INPUT_KEY_TAB,
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_INSERT,
    INPUT_KEY_DELETE,
    INPUT_KEY_RIGHT,
    INPUT_KEY_LEFT,
    INPUT_KEY_DOWN,
    INPUT_KEY_UP,
    INPUT_KEY_PAGE_UP,
    INPUT_KEY_PAGE_DOWN,
    INPUT_KEY_HOME,
    INPUT_KEY_END,
    INPUT_KEY_CAPS_LOCK,
    INPUT_KEY_SCROLL_LOCK,
    INPUT_KEY_NUM_LOCK,
    INPUT_KEY_PRINT_SCREEN,
    INPUT_KEY_PAUSE,
    INPUT_KEY_F1,
    INPUT_KEY_F2,
    INPUT_KEY_F3,
    INPUT_KEY_F4,
    INPUT_KEY_F5,
    INPUT_KEY_F6,
    INPUT_KEY_F7,
    INPUT_KEY_F8,
    INPUT_KEY_F9,
    INPUT_KEY_F10,
    INPUT_KEY_F11,
    INPUT_KEY_F12,
    INPUT_KEY_LEFT_SHIFT,
    INPUT_KEY_LEFT_CONTROL,
    INPUT_KEY_LEFT_ALT,
    INPUT_KEY_LEFT_SUPER,
    INPUT_KEY_RIGHT_SHIFT,
    INPUT_KEY_RIGHT_CONTROL,
    INPUT_KEY_RIGHT_ALT,
    INPUT_KEY_RIGHT_SUPER,
    INPUT_KEY_KB_MENU,
    INPUT_KEY_LEFT_BRACKET,
    INPUT_KEY_RIGHT_BRACKET,
    INPUT_KEY_BACKSLASH,
    INPUT_KEY_SEMICOLON,
    INPUT_KEY_APOSTROPHE,
    INPUT_KEY_MINUS,
    INPUT_KEY_EQUAL,
    INPUT_KEY_GRAVE,
    INPUT_KEY_COMMA,
    INPUT_KEY_PERIOD,
    INPUT_KEY_SLASH,
    INPUT_KEY_SPACE,
    INPUT_KEY_ZERO,
    INPUT_KEY_ONE,
    INPUT_KEY_TWO,
    INPUT_KEY_THREE,
    INPUT_KEY_FOUR,
    INPUT_KEY_FIVE,
    INPUT_KEY_SIX,
    INPUT_KEY_SEVEN,
    INPUT_KEY_EIGHT,
    INPUT_KEY_NINE,
    INPUT_KEY_A,
    INPUT_KEY_B,
    INPUT_KEY_C,
    INPUT_KEY_D,
    INPUT_KEY_E,
    INPUT_KEY_F,
    INPUT_KEY_G,
    INPUT_KEY_H,
    INPUT_KEY_I,
    INPUT_KEY_J,
    INPUT_KEY_K,
    INPUT_KEY_L,
    INPUT_KEY_M,
    INPUT_KEY_N,
    INPUT_KEY_O,
    INPUT_KEY_P,
    INPUT_KEY_Q,
    INPUT_KEY_R,
    INPUT_KEY_S,
    INPUT_KEY_T,
    INPUT_KEY_U,
    INPUT_KEY_V,
    INPUT_KEY_W,
    INPUT_KEY_X,
    INPUT_KEY_Y,
    INPUT_KEY_Z,
    INPUT_KEY_KP_0,
    INPUT_KEY_KP_1,
    INPUT_KEY_KP_2,
    INPUT_KEY_KP_3,
    INPUT_KEY_KP_4,
    INPUT_KEY_KP_5,
    INPUT_KEY_KP_6,
    INPUT_KEY_KP_7,
    INPUT_KEY_KP_8,
    INPUT_KEY_KP_9,
    INPUT_KEY_KP_DECIMAL,
    INPUT_KEY_KP_DIVIDE,
    INPUT_KEY_KP_MULTIPLY,
    INPUT_KEY_KP_SUBTRACT,
    INPUT_KEY_KP_ADD,
    INPUT_KEY_KP_ENTER,
    INPUT_KEY_KP_EQUAL,
    INPUT_KEY_COUNT
} input_key_t;

typedef enum {
    INPUT_MOUSE_LEFT = 0,
    INPUT_MOUSE_RIGHT,
    INPUT_MOUSE_MIDDLE,
    INPUT_MOUSE_BUTTON_COUNT
} input_mouse_t;

/**
 * @brief Pop the oldest key pressed since the last call, or INPUT_KEY_NONE.
 */
int input_key_pressed(void);

bool input_key_down(int key);
bool input_key_up(int key);

bool input_mouse_down(int button);

/**
 * @brief Pointer position within the window, in pixels.
 */
input_point_t input_mouse_position(void);

/**
 * @brief Mouse movement since the previous frame, in pixels.
 *
 * Mirrors input_mouse_position(): a pointer event is one value, so it is returned
 * rather than written through out-parameters.
 */
input_delta_t input_mouse_delta(void);

/**
 * @brief Wheel movement since the previous frame, as whole notches.
 *
 * Sign convention, normalized across backends: positive is a scroll up (away from the
 * user), so a positive value raises the SNES mouse speed. FLTK reports the opposite
 * sign natively and negates it.
 */
int input_mouse_wheel(void);

/**
 * @brief Grab or release the pointer and hide or show its cursor.
 */
void input_mouse_capture(bool capture);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INPUT_H */
