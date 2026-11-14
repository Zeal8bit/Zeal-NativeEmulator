/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/userport/snes_adapter.h"
#include "hw/zeal.h"

static uint8_t snes_adapter_data_pin(uint8_t port)
{
    return port == 0 ? SNES_IO_DATA_1 : SNES_IO_DATA_2;
}

static void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit)
{
    (void)pin;
    (void)bit;
    snes_adapter_t* adapter = &((zeal_t*)pio->machine)->snes_adapter;
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        if (!adapter->attached[port]) {
            pio_set_a_pin(pio, snes_adapter_data_pin(port), 1);
            continue;
        }
        adapter->port_bits[port] >>= 1;
        adapter->port_bits[port] |= 0x80000000;
        pio_set_a_pin(pio, snes_adapter_data_pin(port), adapter->port_bits[port] & 1);
    }
    if (adapter->trace) adapter->trace(adapter->userdata, false, adapter->port_bits);
}

static void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit)
{
    (void)pin;
    (void)bit;
    snes_adapter_t* adapter = &((zeal_t*)pio->machine)->snes_adapter;
    if (adapter->sample) adapter->sample(adapter->userdata, adapter->port_bits, adapter->attached);
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        if (adapter->attached[port]) {
            pio_set_a_pin(pio, snes_adapter_data_pin(port), adapter->port_bits[port] & 1);
        }
    }
    if (adapter->trace) adapter->trace(adapter->userdata, true, adapter->port_bits);
}

int snes_adapter_init(snes_adapter_t* adapter, pio_t* pio)
{
    adapter->pio = pio;
    for (int i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        adapter->port_bits[i] = 0xffff;
    }
    pio_listen_a_pin_change(pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(pio, SNES_IO_CLOCK, 1, snes_adapter_clock);
    return 0;
}

static inline void snes_mouse_set_bit(uint32_t* bits, uint8_t bit, bool value)
{
    // SNES data is active low: logical 1 is driven low, logical 0 is driven high.
    if (value) {
        *bits &= ~(1u << bit);
    } else {
        *bits |= (1u << bit);
    }
}

uint32_t snes_mouse_packet(uint8_t x_mag, bool x_negative, uint8_t y_mag, bool y_negative,
    uint8_t speed, bool left, bool right)
{
    uint32_t bits = 0xffffffff;
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_RIGHT, right);
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_LEFT, left);

    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SPEED_LSB, speed & 0x01);
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SPEED_MSB, (speed >> 1) & 0x01);
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SIGNATURE, true);


    snes_mouse_set_bit(&bits, SNES_MOUSE_Y_SIGN_BIT, y_negative);
    for (uint8_t bit = 0; bit < 7; bit++) {
        snes_mouse_set_bit(&bits, SNES_MOUSE_Y_MAG_SHIFT + bit, (y_mag >> (6 - bit)) & 0x01);
    }

    snes_mouse_set_bit(&bits, SNES_MOUSE_X_SIGN_BIT, x_negative);
    for (uint8_t bit = 0; bit < 7; bit++) {
        snes_mouse_set_bit(&bits, SNES_MOUSE_X_MAG_SHIFT + bit, (x_mag >> (6 - bit)) & 0x01);
    }

    return bits;
}
