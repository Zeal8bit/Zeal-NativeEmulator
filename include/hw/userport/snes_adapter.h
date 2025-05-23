#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"

#define SNES_IO_DATA_1 0
#define SNES_IO_DATA_2 1
#define SNES_IO_LATCH  2
#define SNES_IO_CLOCK  3

typedef struct {
    // // device_t
    device_t parent;
    size_t size; // in bytes
    pio_t *pio;

    // snes specific
    int index; // gamepad index
    bool attached;
    uint16_t bits;
} snes_adapter_t;

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio);
void snes_adapter_attach(snes_adapter_t *snes_adapter, uint8_t index);
void snes_adapter_detatch(snes_adapter_t *snes_adapter);
// uint8_t key_pressed(snes_adapter_t* snes_adapter, uint16_t keycode);

// uint8_t key_released(snes_adapter_t* snes_adapter, uint16_t keycode);
// void snes_adapter_send_next(snes_adapter_t* snes_adapter, pio_t* pio, unsigned long delta);
void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit);
void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit);
