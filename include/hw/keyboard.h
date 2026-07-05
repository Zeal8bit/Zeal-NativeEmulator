/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"
#include "utils/fifo.h"
#include "utils/vtimer.h"

#define IO_KEYBOARD_PIN 7
#define MAX_KEYCODES    10
#define FIFO_SIZE       512
#define BREAK_CODE      0xF0

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


typedef struct {
    // device_t
    device_t    parent;
    size_t      size; // in bytes
    pio_t*      pio;

    // Keyboard specific
    vtimer_node_t timer;            // schedules PS2 state transitions
    uint8_t     shift_register;
    fifo_t      queue;
    uint8_t     pin_state;
    ps2_state_t state;
} keyboard_t;

int keyboard_init(keyboard_t* keyboard, pio_t* pio);
uint8_t key_pressed(keyboard_t* keyboard, uint16_t keycode);
uint8_t key_released(keyboard_t* keyboard, uint16_t keycode);
