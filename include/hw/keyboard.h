/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/host/stdin.h"
#include "hw/pio.h"
#include "utils/fifo.h"

#define IO_KEYBOARD_PIN 7
#define MAX_KEYCODES    10
#define FIFO_SIZE       512
#define BREAK_CODE      0xF0

typedef enum {
    KEYBOARD_MOD_NONE  = 0,
    KEYBOARD_MOD_SHIFT = 1 << 0,
    KEYBOARD_MOD_CTRL  = 1 << 1,
    KEYBOARD_MOD_ALT   = 1 << 2,
} keyboard_modifiers_t;

/**
 * @brief Period, in T-states, to check the host computer keyboard.
 * It is not necessary to check the keyboard events after each Z80
 * instruction. At the same time, it is not good to always check
 * it during v-blank, this introduces predictability. Let it have
 * its own timer.
 */
#define KEYBOARD_CHECK_PERIOD   US_TO_TSTATES(15000)

/**
 * @brief State machine for the keyboard
 */
typedef enum {
    PS2_IDLE     = 0,
    PS2_ACTIVE   = 1,
    PS2_INACTIVE = 2,
} ps2_state_t;

typedef struct keyboard {
    // device_t
    device_t    parent;
    size_t      size; // in bytes
    pio_t*      pio;

    // Host keyboard check timer
    uint32_t    check_timer;

    // Keyboard specific
    uint32_t    elapsed_tstates;    // 32-bit lets us count up to 7 minutes, more than enough
    uint8_t     shift_register;
    fifo_t      queue;
    uint8_t     pin_state;
    ps2_state_t state;

    host_stdin_t host_stdin;
} keyboard_t;

int keyboard_init(keyboard_t* keyboard, pio_t* pio, bool stdin_enabled);
void keyboard_deinit(keyboard_t* keyboard);
bool keyboard_tap_key(keyboard_t* keyboard, uint16_t keycode, uint8_t modifiers);
uint8_t key_pressed(keyboard_t* keyboard, uint16_t keycode);
uint8_t key_released(keyboard_t* keyboard, uint16_t keycode);

/**
 * @brief Tick the internal clock and checks whether the host keyboard
 * must be read or not.
 */
bool keyboard_check(keyboard_t* keyboard, int elapsed);

/**
 * @brief Process the next keypresses if the keyboard is ready.
 */
void keyboard_tick(keyboard_t* keyboard, pio_t* pio, int elapsed);
