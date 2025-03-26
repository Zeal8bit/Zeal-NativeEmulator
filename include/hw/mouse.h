#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"

#define IO_PS2_PIN      7


typedef struct {
    int x;
    int y;
    int z;
    uint8_t lmb;
    uint8_t mmb;
    uint8_t rmb;
} mouse_state_t;


typedef struct {
    device_t parent;
    pio_t* pio;


    /* Just like on the real hardware, give some cycles for the mouse to boot */
    uint32_t booting_elapsed;

    uint32_t state_elapsed;
    mouse_state_t last;
    int diff_x;
    int diff_y;
    /* Number of bytes already sent to the VM */
    bool state_ready;
    uint32_t sent;
    uint32_t sent_elapsed;
    uint8_t shift_register;
} mouse_t;


int mouse_init(mouse_t* mouse, pio_t* pio, bool ps2_interface);
void mouse_send_next(mouse_t* mouse, const mouse_state_t* state, int delta);
void mouse_tick(mouse_t* mouse, int delta);
