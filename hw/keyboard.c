#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "string.h"
#include "raylib.h"
#include "hw/helpers.h"
#include "hw/keyboard.h"
#include "hw/pio.h"

static unsigned long PS2_SCANCODE_DURATION = 0;
static unsigned long PS2_KEY_TIMING = 0;
static unsigned long PS2_RELEASE_DELAY = 0;
static unsigned long elapsed_tstates;

static const uint16_t TABLE[384] = {
    [KEY_BACKSPACE]    = 0x66,
    [KEY_TAB]          = 0x0D,
    [KEY_ENTER]        = 0x5A,
    [KEY_LEFT_SHIFT]   = 0x59,
    [KEY_LEFT_CONTROL] = 0xE014,
    [KEY_LEFT_ALT]     = 0x11,
    [KEY_RIGHT_ALT]    = 0xE011,
    // [KEY_PAUSE] = 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xE0, 0x77,
    [KEY_CAPS_LOCK] = 0x58,
    [KEY_ESCAPE]    = 0x76,
    [KEY_PAGE_UP]   = 0xE07D,
    [KEY_SPACE]     = 0x29,
    [KEY_PAGE_DOWN] = 0xE07A,
    [KEY_END]       = 0xE069,
    [KEY_HOME]      = 0xE06C,
    [KEY_LEFT]      = 0xE06B,
    [KEY_UP]        = 0xE075,
    [KEY_RIGHT]     = 0xE074,
    [KEY_DOWN]      = 0xE072,
    // [KEY_PRINTSCREEN] = 0xE0, 0x12, 0xE0, 0x7C,
    [KEY_INSERT]        = 0xE070,
    [KEY_DELETE]        = 0xE071,
    [KEY_ZERO]          = 0x45,
    [KEY_ONE]           = 0x16,
    [KEY_TWO]           = 0x1E,
    [KEY_THREE]         = 0x26,
    [KEY_FOUR]          = 0x25,
    [KEY_FIVE]          = 0x2E,
    [KEY_SIX]           = 0x36,
    [KEY_SEVEN]         = 0x3D,
    [KEY_EIGHT]         = 0x3E,
    [KEY_NINE]          = 0x46,
    [KEY_A]             = 0x1C,
    [KEY_B]             = 0x32,
    [KEY_C]             = 0x21,
    [KEY_D]             = 0x23,
    [KEY_E]             = 0x24,
    [KEY_F]             = 0x2B,
    [KEY_G]             = 0x34,
    [KEY_H]             = 0x33,
    [KEY_I]             = 0x43,
    [KEY_J]             = 0x3B,
    [KEY_K]             = 0x42,
    [KEY_L]             = 0x4B,
    [KEY_M]             = 0x3A,
    [KEY_N]             = 0x31,
    [KEY_O]             = 0x44,
    [KEY_P]             = 0x4D,
    [KEY_Q]             = 0x15,
    [KEY_R]             = 0x2D,
    [KEY_S]             = 0x1B,
    [KEY_T]             = 0x2C,
    [KEY_U]             = 0x3C,
    [KEY_V]             = 0x2A,
    [KEY_W]             = 0x1D,
    [KEY_X]             = 0x22,
    [KEY_Y]             = 0x35,
    [KEY_Z]             = 0x1A,
    [KEY_LEFT_SUPER]    = 0xE01F,
    [KEY_RIGHT_SUPER]   = 0xE027,
    [KEY_KP_0]          = 0x70,
    [KEY_KP_1]          = 0x69,
    [KEY_KP_2]          = 0x72,
    [KEY_KP_3]          = 0x7A,
    [KEY_KP_4]          = 0x6B,
    [KEY_KP_5]          = 0x73,
    [KEY_KP_6]          = 0x74,
    [KEY_KP_7]          = 0x6C,
    [KEY_KP_8]          = 0x75,
    [KEY_KP_9]          = 0x7D,
    [KEY_KP_MULTIPLY]   = 0x7C,
    [KEY_KP_ADD]        = 0x79,
    [KEY_KP_SUBTRACT]   = 0x7B,
    [KEY_KP_DECIMAL]    = 0x71,
    [KEY_KP_DIVIDE]     = 0xE04A,
    [KEY_F1]            = 0x05,
    [KEY_F2]            = 0x06,
    [KEY_F3]            = 0x04,
    [KEY_F4]            = 0x0C,
    [KEY_F5]            = 0x03,
    [KEY_F6]            = 0x0B,
    [KEY_F7]            = 0x83,
    [KEY_F8]            = 0x0A,
    [KEY_F9]            = 0x01,
    [KEY_F10]           = 0x09,
    [KEY_F11]           = 0x78,
    [KEY_F12]           = 0x07,
    [KEY_NUM_LOCK]      = 0x77,
    [KEY_SCROLL_LOCK]   = 0x7E,
    [KEY_SEMICOLON]     = 0x4C,
    [KEY_EQUAL]         = 0x55,
    [KEY_COMMA]         = 0x41,
    [KEY_MINUS]         = 0x4E,
    [KEY_PERIOD]        = 0x49,
    [KEY_SLASH]         = 0x4A,
    [KEY_LEFT_BRACKET]  = 0x54,
    [KEY_BACKSLASH]     = 0x5D,
    [KEY_RIGHT_BRACKET] = 0x5B,
    [KEY_APOSTROPHE]    = 0x52,
    [KEY_GRAVE]         = 0x0E,
};

static uint8_t io_read(device_t* dev, uint32_t addr)
{
    keyboard_t* keyboard = (keyboard_t*) dev;
    (void) addr;

#if DEBUG
    printf("keyboard:io_read: %02x\n", keyboard->shift_register);
#endif
    return keyboard->shift_register;
}

int keyboard_init(keyboard_t* keyboard, pio_t* pio)
{
    PS2_SCANCODE_DURATION = us_to_tstates(19.7); // 197
    PS2_KEY_TIMING = us_to_tstates(3900); // 39000
    PS2_RELEASE_DELAY = us_to_tstates(30000); // 155000

    keyboard->size = 0x10;
    device_init_io(DEVICE(keyboard), "keyboard_dev", io_read, NULL, keyboard->size);

    keyboard->pin_state = 1;
    assert(fifo_init(&keyboard->queue, FIFO_SIZE));

    pio_set_b_pin(pio, IO_KEYBOARD_PIN, keyboard->pin_state);

    return 0;
}

void keyboard_send_next(keyboard_t* keyboard, pio_t* pio, unsigned long delta)
{
    if(fifo_size(&keyboard->queue) == 0) {
        return; // nothing to process
    }

    /* wait */
    elapsed_tstates += delta;

    if(keyboard->pin_state == 1) {
        if(elapsed_tstates < PS2_KEY_TIMING) return;

        /* Shift in the next code */
        assert(fifo_pop(&keyboard->queue, &keyboard->shift_register));
        keyboard->pin_state = 0;
        pio_set_b_pin(pio, IO_KEYBOARD_PIN, keyboard->pin_state);

#if DEBUG
        printf("keyboard_send_next:0: toggle pin_state %d at %zu [%zu]\n", keyboard->pin_state, keyboard->fifo_position, elapsed_tstates);
#endif
        return;
    }

    if(elapsed_tstates < PS2_KEY_TIMING + PS2_SCANCODE_DURATION) return;

    keyboard->pin_state = 1;
    pio_set_b_pin(pio, IO_KEYBOARD_PIN, keyboard->pin_state);
#if DEBUG
    printf("keyboard_send_next:1: toggle pin_state %d at %zu [%zu]\n", keyboard->pin_state, keyboard->fifo_position, elapsed_tstates);
#endif

    // /* reached the end? */
    if (keyboard->fifo_position > keyboard->fifo_length) {
        keyboard->fifo_length = 0;
        keyboard->fifo_position = 0;
#if DEBUG
        printf("keyboard_send_next: clear fifo\n");
#endif
    }

    elapsed_tstates = 0;
}

static uint8_t get_ps2_code(uint16_t keycode, uint8_t* codes)
{
    switch (keycode) {
        case KEY_PAUSE: {
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
        case KEY_PRINT_SCREEN: {
            codes[0] = 0xE0;
            codes[0] = 0x12;
            codes[0] = 0xE0;
            codes[0] = 0x7C;
            return 4;
        } break;
        default: {
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
    // printf("key_pressed: fifo_length %zu\n", keyboard->fifo_length);
    uint8_t codes[MAX_KEYCODES];
    int n_codes = get_ps2_code(keycode, codes);
    for (int i = 0; i < n_codes; i++) {
        fifo_push(&keyboard->queue, codes[i]);
    }

    return 0;
}

uint8_t key_released(keyboard_t* keyboard, uint16_t keycode)
{
    /* PAUSE has no break code */
    if (keycode == KEY_PAUSE) {
        return 0;
    }

    // TODO: notify caller that the result wasn't handled?
    if (keyboard->fifo_length >= FIFO_SIZE) {
        return 1;
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

    return 0;
}
