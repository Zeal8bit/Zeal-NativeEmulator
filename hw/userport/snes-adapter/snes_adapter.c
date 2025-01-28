#include <stdint.h>
#include <stdio.h>

#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes-adapter/controller.h"
#include "hw/pio.h"
#include "hw/zeal.h"

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio) {
    snes_adapter->size = 0x00;
    snes_adapter->pio = pio;

    snes_controller_load_mappings();

    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        snes_controller_init(&snes_adapter->controllers[i], i);
    }


    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        if (snes_controller_available(i)) {
            printf("[SNES] Found \"%s\"\n", snes_controller_name(i));
            snes_adapter_attach(snes_adapter, i);
        }
    }

    return 0;
}

void snes_adapter_attach(snes_adapter_t* snes_adapter, uint8_t index) {
    snes_adapter->controllers[index].index = index;
    snes_adapter->controllers[index].attached = true;
    // Listeners are shared for both controllers; registering again is safe (overwrites same slot)
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK, 1, snes_adapter_clock);

    printf("[SNES] Attached \"%s\" to port %d\n", snes_controller_name(index), index);
}

void snes_adapter_detatch(snes_adapter_t* snes_adapter) {
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}

void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    // Snapshot button state for both controllers simultaneously on latch
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        snes_adapter->controllers[port].bits = snes_controller_latch(&snes_adapter->controllers[port]);
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->controllers[0].bits & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->controllers[1].bits & 0x01);
}

void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    // Advance both controllers' bit streams in lock-step
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        snes_adapter->controllers[port].bits >>= 1;
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->controllers[0].bits & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->controllers[1].bits & 0x01);

    // printf("[SNES] clock: bits[0]: %04x bits[1]: %04x\n", snes_adapter->bits[0], snes_adapter->bits[1]);
}
