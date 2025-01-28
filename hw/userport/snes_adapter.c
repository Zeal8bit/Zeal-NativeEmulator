#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "raylib.h"
#include "utils/helpers.h"
#include "hw/userport/snes_adapter.h"
#include "hw/pio.h"

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio)
{
    snes_adapter->size = 0x00;
    snes_adapter->pio = pio;

    snes_adapter->bits = 0x0000;
    snes_adapter->bitCounter = 0;
    snes_adapter->index = 0;

    if(IsGamepadAvailable(snes_adapter->index)) {
        printf("[SNES Adapter] Found %s\n", GetGamepadName(0));
    }

    return 0;
}

void snes_adapter_attach(snes_adapter_t *snes_adapter, uint8_t index) {
    snes_adapter->index = index;
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK, 1, snes_adapter_clock);

    printf("[SNES Adapter] Attached %d \"%s\"\n", index, GetGamepadName(index));
}

void snes_adapter_detatch(snes_adapter_t *snes_adapter) {
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}

void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit) {
    (void)pio;
    (void)pin;
    (void)bit;
}

void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit) {
    (void)pio;
    (void)pin;
    (void)bit;
}
