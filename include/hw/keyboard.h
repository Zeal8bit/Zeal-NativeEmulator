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


typedef struct {
    // device_t
    device_t parent;
    size_t size; // in bytes

    // keyboard specific
    uint8_t shift_register;
    fifo_t queue;
    uint8_t pin_state;
} keyboard_t;

int keyboard_init(keyboard_t* keyboard, pio_t* pio);
uint8_t key_pressed(keyboard_t* keyboard, uint16_t keycode);
uint8_t key_released(keyboard_t* keyboard, uint16_t keycode);
void keyboard_send_next(keyboard_t* keyboard, pio_t* pio, unsigned long delta);
