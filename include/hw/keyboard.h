#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"
#include "utils/fifo.h"

#define IO_KEYBOARD_PIN 7
#define MAX_KEYCODES    10
#define FIFO_SIZE       512
#define BREAK_CODE      0xF0


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

    // Keyboard specific
    uint32_t    elapsed_tstates;    // 32-bit lets us count up to 7 minutes, more than enough
    uint8_t     shift_register;
    fifo_t      queue;
    uint8_t     pin_state;
    ps2_state_t state;
} keyboard_t;

int keyboard_init(keyboard_t* keyboard, pio_t* pio);
uint8_t key_pressed(keyboard_t* keyboard, uint16_t keycode);
uint8_t key_released(keyboard_t* keyboard, uint16_t keycode);
void keyboard_send_next(keyboard_t* keyboard, pio_t* pio, int delta);
