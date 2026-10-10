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

#include "platform/display.h"
#include "utils/helpers.h"
#include "hw/keyboard.h"
#include "hw/pio.h"

static unsigned long PS2_SCANCODE_DURATION = 0;
static unsigned long PS2_KEY_TIMING = 0;


static const uint16_t TABLE[384] = {
    [DISPLAY_KEY_BACKSPACE]    = 0x66,
    [DISPLAY_KEY_TAB]          = 0x0D,
    [DISPLAY_KEY_ENTER]        = 0x5A,
    [DISPLAY_KEY_LEFT_SHIFT]   = 0x12,
    [DISPLAY_KEY_RIGHT_SHIFT]  = 0x59,
    [DISPLAY_KEY_RIGHT_CONTROL] = 0xE014,
    [DISPLAY_KEY_KP_ENTER] = 0xE05A,
    [DISPLAY_KEY_LEFT_CONTROL] = 0xE014,
    [DISPLAY_KEY_LEFT_ALT]     = 0x11,
    [DISPLAY_KEY_RIGHT_ALT]    = 0xE011,
    // [DISPLAY_KEY_PAUSE] = 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xE0, 0x77,
    [DISPLAY_KEY_CAPS_LOCK] = 0x58,
    [DISPLAY_KEY_ESCAPE]    = 0x76,
    [DISPLAY_KEY_PAGE_UP]   = 0xE07D,
    [DISPLAY_KEY_SPACE]     = 0x29,
    [DISPLAY_KEY_PAGE_DOWN] = 0xE07A,
    [DISPLAY_KEY_END]       = 0xE069,
    [DISPLAY_KEY_HOME]      = 0xE06C,
    [DISPLAY_KEY_LEFT]      = 0xE06B,
    [DISPLAY_KEY_UP]        = 0xE075,
    [DISPLAY_KEY_RIGHT]     = 0xE074,
    [DISPLAY_KEY_DOWN]      = 0xE072,
    // [DISPLAY_KEY_PRINT_SCREEN] = 0xE0, 0x12, 0xE0, 0x7C,
    [DISPLAY_KEY_INSERT]        = 0xE070,
    [DISPLAY_KEY_DELETE]        = 0xE071,
    [DISPLAY_KEY_ZERO]          = 0x45,
    [DISPLAY_KEY_ONE]           = 0x16,
    [DISPLAY_KEY_TWO]           = 0x1E,
    [DISPLAY_KEY_THREE]         = 0x26,
    [DISPLAY_KEY_FOUR]          = 0x25,
    [DISPLAY_KEY_FIVE]          = 0x2E,
    [DISPLAY_KEY_SIX]           = 0x36,
    [DISPLAY_KEY_SEVEN]         = 0x3D,
    [DISPLAY_KEY_EIGHT]         = 0x3E,
    [DISPLAY_KEY_NINE]          = 0x46,
    [DISPLAY_KEY_A]             = 0x1C,
    [DISPLAY_KEY_B]             = 0x32,
    [DISPLAY_KEY_C]             = 0x21,
    [DISPLAY_KEY_D]             = 0x23,
    [DISPLAY_KEY_E]             = 0x24,
    [DISPLAY_KEY_F]             = 0x2B,
    [DISPLAY_KEY_G]             = 0x34,
    [DISPLAY_KEY_H]             = 0x33,
    [DISPLAY_KEY_I]             = 0x43,
    [DISPLAY_KEY_J]             = 0x3B,
    [DISPLAY_KEY_K]             = 0x42,
    [DISPLAY_KEY_L]             = 0x4B,
    [DISPLAY_KEY_M]             = 0x3A,
    [DISPLAY_KEY_N]             = 0x31,
    [DISPLAY_KEY_O]             = 0x44,
    [DISPLAY_KEY_P]             = 0x4D,
    [DISPLAY_KEY_Q]             = 0x15,
    [DISPLAY_KEY_R]             = 0x2D,
    [DISPLAY_KEY_S]             = 0x1B,
    [DISPLAY_KEY_T]             = 0x2C,
    [DISPLAY_KEY_U]             = 0x3C,
    [DISPLAY_KEY_V]             = 0x2A,
    [DISPLAY_KEY_W]             = 0x1D,
    [DISPLAY_KEY_X]             = 0x22,
    [DISPLAY_KEY_Y]             = 0x35,
    [DISPLAY_KEY_Z]             = 0x1A,
    [DISPLAY_KEY_LEFT_SUPER]    = 0xE01F,
    [DISPLAY_KEY_RIGHT_SUPER]   = 0xE027,
    [DISPLAY_KEY_KP_0]          = 0x70,
    [DISPLAY_KEY_KP_1]          = 0x69,
    [DISPLAY_KEY_KP_2]          = 0x72,
    [DISPLAY_KEY_KP_3]          = 0x7A,
    [DISPLAY_KEY_KP_4]          = 0x6B,
    [DISPLAY_KEY_KP_5]          = 0x73,
    [DISPLAY_KEY_KP_6]          = 0x74,
    [DISPLAY_KEY_KP_7]          = 0x6C,
    [DISPLAY_KEY_KP_8]          = 0x75,
    [DISPLAY_KEY_KP_9]          = 0x7D,
    [DISPLAY_KEY_KP_MULTIPLY]   = 0x7C,
    [DISPLAY_KEY_KP_ADD]        = 0x79,
    [DISPLAY_KEY_KP_SUBTRACT]   = 0x7B,
    [DISPLAY_KEY_KP_DECIMAL]    = 0x71,
    [DISPLAY_KEY_KP_DIVIDE]     = 0xE04A,
    [DISPLAY_KEY_F1]            = 0x05,
    [DISPLAY_KEY_F2]            = 0x06,
    [DISPLAY_KEY_F3]            = 0x04,
    [DISPLAY_KEY_F4]            = 0x0C,
    [DISPLAY_KEY_F5]            = 0x03,
    [DISPLAY_KEY_F6]            = 0x0B,
    [DISPLAY_KEY_F7]            = 0x83,
    [DISPLAY_KEY_F8]            = 0x0A,
    [DISPLAY_KEY_F9]            = 0x01,
    [DISPLAY_KEY_F10]           = 0x09,
    [DISPLAY_KEY_F11]           = 0x78,
    [DISPLAY_KEY_F12]           = 0x07,
    [DISPLAY_KEY_NUM_LOCK]      = 0x77,
    [DISPLAY_KEY_SCROLL_LOCK]   = 0x7E,
    [DISPLAY_KEY_SEMICOLON]     = 0x4C,
    [DISPLAY_KEY_EQUAL]         = 0x55,
    [DISPLAY_KEY_COMMA]         = 0x41,
    [DISPLAY_KEY_MINUS]         = 0x4E,
    [DISPLAY_KEY_PERIOD]        = 0x49,
    [DISPLAY_KEY_SLASH]         = 0x4A,
    [DISPLAY_KEY_LEFT_BRACKET]  = 0x54,
    [DISPLAY_KEY_BACKSLASH]     = 0x5D,
    [DISPLAY_KEY_RIGHT_BRACKET] = 0x5B,
    [DISPLAY_KEY_APOSTROPHE]    = 0x52,
    [DISPLAY_KEY_GRAVE]         = 0x0E,
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
        case DISPLAY_KEY_PAUSE: {
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
        case DISPLAY_KEY_PRINT_SCREEN: {
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
    if (keycode == DISPLAY_KEY_PAUSE) {
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
