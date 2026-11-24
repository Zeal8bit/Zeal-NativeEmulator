/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
/* Stable logical key IDs accepted by the PS/2 encoder. Host key translation
 * belongs to the frontend; these values preserve the existing input API. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZEAL_KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    ZEAL_KEY_APOSTROPHE      = 39,       // Key: '
    ZEAL_KEY_COMMA           = 44,       // Key: ,
    ZEAL_KEY_MINUS           = 45,       // Key: -
    ZEAL_KEY_PERIOD          = 46,       // Key: .
    ZEAL_KEY_SLASH           = 47,       // Key: /
    ZEAL_KEY_ZERO            = 48,       // Key: 0
    ZEAL_KEY_ONE             = 49,       // Key: 1
    ZEAL_KEY_TWO             = 50,       // Key: 2
    ZEAL_KEY_THREE           = 51,       // Key: 3
    ZEAL_KEY_FOUR            = 52,       // Key: 4
    ZEAL_KEY_FIVE            = 53,       // Key: 5
    ZEAL_KEY_SIX             = 54,       // Key: 6
    ZEAL_KEY_SEVEN           = 55,       // Key: 7
    ZEAL_KEY_EIGHT           = 56,       // Key: 8
    ZEAL_KEY_NINE            = 57,       // Key: 9
    ZEAL_KEY_SEMICOLON       = 59,       // Key: ;
    ZEAL_KEY_EQUAL           = 61,       // Key: =
    ZEAL_KEY_A               = 65,       // Key: A | a
    ZEAL_KEY_B               = 66,       // Key: B | b
    ZEAL_KEY_C               = 67,       // Key: C | c
    ZEAL_KEY_D               = 68,       // Key: D | d
    ZEAL_KEY_E               = 69,       // Key: E | e
    ZEAL_KEY_F               = 70,       // Key: F | f
    ZEAL_KEY_G               = 71,       // Key: G | g
    ZEAL_KEY_H               = 72,       // Key: H | h
    ZEAL_KEY_I               = 73,       // Key: I | i
    ZEAL_KEY_J               = 74,       // Key: J | j
    ZEAL_KEY_K               = 75,       // Key: K | k
    ZEAL_KEY_L               = 76,       // Key: L | l
    ZEAL_KEY_M               = 77,       // Key: M | m
    ZEAL_KEY_N               = 78,       // Key: N | n
    ZEAL_KEY_O               = 79,       // Key: O | o
    ZEAL_KEY_P               = 80,       // Key: P | p
    ZEAL_KEY_Q               = 81,       // Key: Q | q
    ZEAL_KEY_R               = 82,       // Key: R | r
    ZEAL_KEY_S               = 83,       // Key: S | s
    ZEAL_KEY_T               = 84,       // Key: T | t
    ZEAL_KEY_U               = 85,       // Key: U | u
    ZEAL_KEY_V               = 86,       // Key: V | v
    ZEAL_KEY_W               = 87,       // Key: W | w
    ZEAL_KEY_X               = 88,       // Key: X | x
    ZEAL_KEY_Y               = 89,       // Key: Y | y
    ZEAL_KEY_Z               = 90,       // Key: Z | z
    ZEAL_KEY_LEFT_BRACKET    = 91,       // Key: [
    ZEAL_KEY_BACKSLASH       = 92,       // Key: '\'
    ZEAL_KEY_RIGHT_BRACKET   = 93,       // Key: ]
    ZEAL_KEY_GRAVE           = 96,       // Key: `
    // Function keys
    ZEAL_KEY_SPACE           = 32,       // Key: Space
    ZEAL_KEY_ESCAPE          = 256,      // Key: Esc
    ZEAL_KEY_ENTER           = 257,      // Key: Enter
    ZEAL_KEY_TAB             = 258,      // Key: Tab
    ZEAL_KEY_BACKSPACE       = 259,      // Key: Backspace
    ZEAL_KEY_INSERT          = 260,      // Key: Ins
    ZEAL_KEY_DELETE          = 261,      // Key: Del
    ZEAL_KEY_RIGHT           = 262,      // Key: Cursor right
    ZEAL_KEY_LEFT            = 263,      // Key: Cursor left
    ZEAL_KEY_DOWN            = 264,      // Key: Cursor down
    ZEAL_KEY_UP              = 265,      // Key: Cursor up
    ZEAL_KEY_PAGE_UP         = 266,      // Key: Page up
    ZEAL_KEY_PAGE_DOWN       = 267,      // Key: Page down
    ZEAL_KEY_HOME            = 268,      // Key: Home
    ZEAL_KEY_END             = 269,      // Key: End
    ZEAL_KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    ZEAL_KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    ZEAL_KEY_NUM_LOCK        = 282,      // Key: Num lock
    ZEAL_KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    ZEAL_KEY_PAUSE           = 284,      // Key: Pause
    ZEAL_KEY_F1              = 290,      // Key: F1
    ZEAL_KEY_F2              = 291,      // Key: F2
    ZEAL_KEY_F3              = 292,      // Key: F3
    ZEAL_KEY_F4              = 293,      // Key: F4
    ZEAL_KEY_F5              = 294,      // Key: F5
    ZEAL_KEY_F6              = 295,      // Key: F6
    ZEAL_KEY_F7              = 296,      // Key: F7
    ZEAL_KEY_F8              = 297,      // Key: F8
    ZEAL_KEY_F9              = 298,      // Key: F9
    ZEAL_KEY_F10             = 299,      // Key: F10
    ZEAL_KEY_F11             = 300,      // Key: F11
    ZEAL_KEY_F12             = 301,      // Key: F12
    ZEAL_KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    ZEAL_KEY_LEFT_CONTROL    = 341,      // Key: Control left
    ZEAL_KEY_LEFT_ALT        = 342,      // Key: Alt left
    ZEAL_KEY_LEFT_SUPER      = 343,      // Key: Super left
    ZEAL_KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    ZEAL_KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    ZEAL_KEY_RIGHT_ALT       = 346,      // Key: Alt right
    ZEAL_KEY_RIGHT_SUPER     = 347,      // Key: Super right
    ZEAL_KEY_KB_MENU         = 348,      // Key: KB menu
    // Keypad keys
    ZEAL_KEY_KP_0            = 320,      // Key: Keypad 0
    ZEAL_KEY_KP_1            = 321,      // Key: Keypad 1
    ZEAL_KEY_KP_2            = 322,      // Key: Keypad 2
    ZEAL_KEY_KP_3            = 323,      // Key: Keypad 3
    ZEAL_KEY_KP_4            = 324,      // Key: Keypad 4
    ZEAL_KEY_KP_5            = 325,      // Key: Keypad 5
    ZEAL_KEY_KP_6            = 326,      // Key: Keypad 6
    ZEAL_KEY_KP_7            = 327,      // Key: Keypad 7
    ZEAL_KEY_KP_8            = 328,      // Key: Keypad 8
    ZEAL_KEY_KP_9            = 329,      // Key: Keypad 9
    ZEAL_KEY_KP_DECIMAL      = 330,      // Key: Keypad .
    ZEAL_KEY_KP_DIVIDE       = 331,      // Key: Keypad /
    ZEAL_KEY_KP_MULTIPLY     = 332,      // Key: Keypad *
    ZEAL_KEY_KP_SUBTRACT     = 333,      // Key: Keypad -
    ZEAL_KEY_KP_ADD          = 334,      // Key: Keypad +
    ZEAL_KEY_KP_ENTER        = 335,      // Key: Keypad Enter
    ZEAL_KEY_KP_EQUAL        = 336,      // Key: Keypad =
} zeal_key_t;

#ifdef __cplusplus
} /* extern "C" */
#endif
