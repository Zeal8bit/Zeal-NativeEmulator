/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "host/zeal_host.h"
#include "utils/helpers.h"
#include "hw/keyboard.h"
#include "hw/pio.h"

static unsigned long PS2_SCANCODE_DURATION = 0;
static unsigned long PS2_KEY_TIMING = 0;


static const uint16_t TABLE[384] = {
    [ZEAL_HOST_KEY_BACKSPACE]    = 0x66,
    [ZEAL_HOST_KEY_TAB]          = 0x0D,
    [ZEAL_HOST_KEY_ENTER]        = 0x5A,
    [ZEAL_HOST_KEY_LEFT_SHIFT]   = 0x12,
    [ZEAL_HOST_KEY_RIGHT_SHIFT]  = 0x59,
    [ZEAL_HOST_KEY_RIGHT_CONTROL] = 0xE014,
    [ZEAL_HOST_KEY_KP_ENTER] = 0xE05A,
    [ZEAL_HOST_KEY_LEFT_CONTROL] = 0xE014,
    [ZEAL_HOST_KEY_LEFT_ALT]     = 0x11,
    [ZEAL_HOST_KEY_RIGHT_ALT]    = 0xE011,
    // [ZEAL_HOST_KEY_PAUSE] = 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xE0, 0x77,
    [ZEAL_HOST_KEY_CAPS_LOCK] = 0x58,
    [ZEAL_HOST_KEY_ESCAPE]    = 0x76,
    [ZEAL_HOST_KEY_PAGE_UP]   = 0xE07D,
    [ZEAL_HOST_KEY_SPACE]     = 0x29,
    [ZEAL_HOST_KEY_PAGE_DOWN] = 0xE07A,
    [ZEAL_HOST_KEY_END]       = 0xE069,
    [ZEAL_HOST_KEY_HOME]      = 0xE06C,
    [ZEAL_HOST_KEY_LEFT]      = 0xE06B,
    [ZEAL_HOST_KEY_UP]        = 0xE075,
    [ZEAL_HOST_KEY_RIGHT]     = 0xE074,
    [ZEAL_HOST_KEY_DOWN]      = 0xE072,
    // [ZEAL_HOST_KEY_PRINT_SCREEN] = 0xE0, 0x12, 0xE0, 0x7C,
    [ZEAL_HOST_KEY_INSERT]        = 0xE070,
    [ZEAL_HOST_KEY_DELETE]        = 0xE071,
    [ZEAL_HOST_KEY_ZERO]          = 0x45,
    [ZEAL_HOST_KEY_ONE]           = 0x16,
    [ZEAL_HOST_KEY_TWO]           = 0x1E,
    [ZEAL_HOST_KEY_THREE]         = 0x26,
    [ZEAL_HOST_KEY_FOUR]          = 0x25,
    [ZEAL_HOST_KEY_FIVE]          = 0x2E,
    [ZEAL_HOST_KEY_SIX]           = 0x36,
    [ZEAL_HOST_KEY_SEVEN]         = 0x3D,
    [ZEAL_HOST_KEY_EIGHT]         = 0x3E,
    [ZEAL_HOST_KEY_NINE]          = 0x46,
    [ZEAL_HOST_KEY_A]             = 0x1C,
    [ZEAL_HOST_KEY_B]             = 0x32,
    [ZEAL_HOST_KEY_C]             = 0x21,
    [ZEAL_HOST_KEY_D]             = 0x23,
    [ZEAL_HOST_KEY_E]             = 0x24,
    [ZEAL_HOST_KEY_F]             = 0x2B,
    [ZEAL_HOST_KEY_G]             = 0x34,
    [ZEAL_HOST_KEY_H]             = 0x33,
    [ZEAL_HOST_KEY_I]             = 0x43,
    [ZEAL_HOST_KEY_J]             = 0x3B,
    [ZEAL_HOST_KEY_K]             = 0x42,
    [ZEAL_HOST_KEY_L]             = 0x4B,
    [ZEAL_HOST_KEY_M]             = 0x3A,
    [ZEAL_HOST_KEY_N]             = 0x31,
    [ZEAL_HOST_KEY_O]             = 0x44,
    [ZEAL_HOST_KEY_P]             = 0x4D,
    [ZEAL_HOST_KEY_Q]             = 0x15,
    [ZEAL_HOST_KEY_R]             = 0x2D,
    [ZEAL_HOST_KEY_S]             = 0x1B,
    [ZEAL_HOST_KEY_T]             = 0x2C,
    [ZEAL_HOST_KEY_U]             = 0x3C,
    [ZEAL_HOST_KEY_V]             = 0x2A,
    [ZEAL_HOST_KEY_W]             = 0x1D,
    [ZEAL_HOST_KEY_X]             = 0x22,
    [ZEAL_HOST_KEY_Y]             = 0x35,
    [ZEAL_HOST_KEY_Z]             = 0x1A,
    [ZEAL_HOST_KEY_LEFT_SUPER]    = 0xE01F,
    [ZEAL_HOST_KEY_RIGHT_SUPER]   = 0xE027,
    [ZEAL_HOST_KEY_KP_0]          = 0x70,
    [ZEAL_HOST_KEY_KP_1]          = 0x69,
    [ZEAL_HOST_KEY_KP_2]          = 0x72,
    [ZEAL_HOST_KEY_KP_3]          = 0x7A,
    [ZEAL_HOST_KEY_KP_4]          = 0x6B,
    [ZEAL_HOST_KEY_KP_5]          = 0x73,
    [ZEAL_HOST_KEY_KP_6]          = 0x74,
    [ZEAL_HOST_KEY_KP_7]          = 0x6C,
    [ZEAL_HOST_KEY_KP_8]          = 0x75,
    [ZEAL_HOST_KEY_KP_9]          = 0x7D,
    [ZEAL_HOST_KEY_KP_MULTIPLY]   = 0x7C,
    [ZEAL_HOST_KEY_KP_ADD]        = 0x79,
    [ZEAL_HOST_KEY_KP_SUBTRACT]   = 0x7B,
    [ZEAL_HOST_KEY_KP_DECIMAL]    = 0x71,
    [ZEAL_HOST_KEY_KP_DIVIDE]     = 0xE04A,
    [ZEAL_HOST_KEY_F1]            = 0x05,
    [ZEAL_HOST_KEY_F2]            = 0x06,
    [ZEAL_HOST_KEY_F3]            = 0x04,
    [ZEAL_HOST_KEY_F4]            = 0x0C,
    [ZEAL_HOST_KEY_F5]            = 0x03,
    [ZEAL_HOST_KEY_F6]            = 0x0B,
    [ZEAL_HOST_KEY_F7]            = 0x83,
    [ZEAL_HOST_KEY_F8]            = 0x0A,
    [ZEAL_HOST_KEY_F9]            = 0x01,
    [ZEAL_HOST_KEY_F10]           = 0x09,
    [ZEAL_HOST_KEY_F11]           = 0x78,
    [ZEAL_HOST_KEY_F12]           = 0x07,
    [ZEAL_HOST_KEY_NUM_LOCK]      = 0x77,
    [ZEAL_HOST_KEY_SCROLL_LOCK]   = 0x7E,
    [ZEAL_HOST_KEY_SEMICOLON]     = 0x4C,
    [ZEAL_HOST_KEY_EQUAL]         = 0x55,
    [ZEAL_HOST_KEY_COMMA]         = 0x41,
    [ZEAL_HOST_KEY_MINUS]         = 0x4E,
    [ZEAL_HOST_KEY_PERIOD]        = 0x49,
    [ZEAL_HOST_KEY_SLASH]         = 0x4A,
    [ZEAL_HOST_KEY_LEFT_BRACKET]  = 0x54,
    [ZEAL_HOST_KEY_BACKSLASH]     = 0x5D,
    [ZEAL_HOST_KEY_RIGHT_BRACKET] = 0x5B,
    [ZEAL_HOST_KEY_APOSTROPHE]    = 0x52,
    [ZEAL_HOST_KEY_GRAVE]         = 0x0E,
};

static uint8_t io_read(device_t* dev, uint32_t addr)
{
    keyboard_t* keyboard = (keyboard_t*) dev;
    (void) addr;

    return keyboard->shift_register;
}


static void keyboard_reset(device_t* dev)
{
    keyboard_t* keyboard = (keyboard_t*) dev;
    keyboard->pin_state = 1;
    keyboard->state = PS2_IDLE;
    keyboard->shift_register = 0;
    vtimer_cancel(&keyboard->timer);
    pio_set_b_pin(keyboard->pio, IO_KEYBOARD_PIN, keyboard->pin_state);
    fifo_reset(&keyboard->queue);
}


/**
 * @brief Function called after a key was pushed to the FIFO, it will go to the next FSM state if currenlty in
 * IDLE and schedule the timer.
 */
static void keyboard_key_available(keyboard_t* keyboard)
{
    if (keyboard->state == PS2_IDLE) {
        fifo_pop(&keyboard->queue, &keyboard->shift_register);
        keyboard->pin_state = 0;
        pio_set_b_pin(keyboard->pio, IO_KEYBOARD_PIN, keyboard->pin_state);
        keyboard->state = PS2_ACTIVE;
        vtimer_schedule_tstates(&keyboard->timer, PS2_SCANCODE_DURATION);
    }
}


/**
 * @brief Callback fired by vtimer when a PS2 state transition is due.
 */
static void keyboard_tick_cb(void* userdata)
{
    keyboard_t* kb = (keyboard_t*) userdata;

    switch (kb->state) {
        case PS2_ACTIVE:
            /* End of start bit: pin goes high, enter inactive gap */
            kb->pin_state = 1;
            pio_set_b_pin(kb->pio, IO_KEYBOARD_PIN, kb->pin_state);
            kb->state = PS2_INACTIVE;
            vtimer_schedule_tstates(&kb->timer, PS2_KEY_TIMING);
            break;

        case PS2_INACTIVE:
            /* Gap between bytes complete, back to idle. Try to send next key. */
            if (fifo_pop(&kb->queue, &kb->shift_register)) {
                kb->pin_state = 0;
                pio_set_b_pin(kb->pio, IO_KEYBOARD_PIN, kb->pin_state);
                kb->state = PS2_ACTIVE;
                vtimer_schedule_tstates(&kb->timer, PS2_SCANCODE_DURATION);
            } else {
                kb->state = PS2_IDLE;
            }
            break;
        default:
            break;
    }
}


int keyboard_init(keyboard_t* keyboard, pio_t* pio)
{
    /* On the real hardware, the active signal stays on for ~19.7 microseconds */
    PS2_SCANCODE_DURATION = us_to_tstates(19.7);
    /* We have a delay of 3.9ms between each scancode */
    PS2_KEY_TIMING = us_to_tstates(3900); // 39000
    /* The release code happens 30ms after the first code is issued */

    keyboard->pio = pio;
    keyboard->size = 0x10;
    vtimer_init_node(&keyboard->timer, keyboard_tick_cb, keyboard);
    device_init_io(DEVICE(keyboard), "keyboard_dev", io_read, NULL, keyboard->size);
    device_register_reset(DEVICE(keyboard), keyboard_reset);

    assert(fifo_init(&keyboard->queue, FIFO_SIZE));
    keyboard_reset(DEVICE(keyboard));

    return 0;
}

static uint8_t get_ps2_code(uint16_t keycode, uint8_t* codes)
{
    switch (keycode) {
        case ZEAL_HOST_KEY_PAUSE: {
            codes[0] = 0xE1;
            codes[1] = 0x14;
            codes[2] = 0x77;
            codes[3] = 0xE1;
            codes[4] = 0xF0;
            codes[5] = 0x14;
            codes[6] = 0xE0;
            codes[7] = 0x77;
            return 8;
        } break;
        case ZEAL_HOST_KEY_PRINT_SCREEN: {
            codes[0] = 0xE0;
            codes[0] = 0x12;
            codes[0] = 0xE0;
            codes[0] = 0x7C;
            return 4;
        } break;
        default: {
            if (keycode >= sizeof(TABLE)/sizeof(TABLE[0]) || !TABLE[keycode]) return 0;
            uint16_t code = TABLE[keycode];
            if (code > 256) {
                codes[0] = code >> 8;
                codes[1] = code & 0xFF;
                return 2;
            }
            codes[0] = code;
            return 1;
        }
    }
    return 0;
}

uint8_t key_pressed(keyboard_t* keyboard, uint16_t keycode)
{
    uint8_t codes[MAX_KEYCODES];
    int n_codes = get_ps2_code(keycode, codes);
    if (!n_codes) return 1;
    for (int i = 0; i < n_codes; i++) {
        fifo_push(&keyboard->queue, codes[i]);
    }
    keyboard_key_available(keyboard);

    return 0;
}

uint8_t key_released(keyboard_t* keyboard, uint16_t keycode)
{
    /* PAUSE has no break code */
    if (keycode == ZEAL_HOST_KEY_PAUSE) {
        return 0;
    }

    uint8_t codes[MAX_KEYCODES];
    int n_codes = get_ps2_code(keycode, codes);
    if (n_codes < 1) {
        return 1;
    }

    /* If the first keycode is `0xE0`, we have to send `0E0` before BREAK_CODE */
    uint8_t* from = codes;
    if (codes[0] == 0xE0) {
        fifo_push(&keyboard->queue, codes[0]);
        n_codes--;
        from++;
    }
    fifo_push(&keyboard->queue, BREAK_CODE);
    for (int i = 0; i < n_codes; i++) {
        fifo_push(&keyboard->queue, from[i]);
    }
    keyboard_key_available(keyboard);

    return 0;
}
