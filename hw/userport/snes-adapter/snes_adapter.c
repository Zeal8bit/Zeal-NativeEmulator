#include <stdint.h>
#include <stdio.h>

#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes-adapter/controller.h"
#include "hw/userport/snes-adapter/mouse.h"
#include "hw/pio.h"
#include "hw/zeal.h"


static void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit)
{
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    // Advance both ports' bit streams in lock-step
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        snes_adapter->port_bits[port] >>= 1;
        snes_adapter->port_bits[port] |= 0x80000000;
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->port_bits[0] & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->port_bits[1] & 0x01);
}


static void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit)
{
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        switch (snes_adapter->ports[port].device) {
            case SNES_PORT_DEVICE_MOUSE:
                snes_adapter->port_bits[port] = snes_mouse_latch(&snes_adapter->mouse);
                break;
            case SNES_PORT_DEVICE_CONTROLLER:
                snes_adapter->port_bits[port] =
                    snes_controller_latch(&snes_adapter->controllers[snes_adapter->ports[port].controller_index]) | 0xFFFF0000;
                break;
            case SNES_PORT_DEVICE_DETACHED:
            default:
                snes_adapter->port_bits[port] = 0xFFFFFFFF;
                break;
        }
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->port_bits[0] & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->port_bits[1] & 0x01);
}


static void snes_adapter_listen(snes_adapter_t* snes_adapter)
{
    // Listeners are shared for both ports; registering again is safe (overwrites same slot)
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK, 1, snes_adapter_clock);
}

static void snes_adapter_detach_port(snes_adapter_t* snes_adapter, uint8_t port)
{
    snes_port_assignment_t* assignment = &snes_adapter->ports[port];

    if (assignment->device == SNES_PORT_DEVICE_CONTROLLER) {
        int index = assignment->controller_index;

        if (index >= 0 && index < SNES_GAMEPAD_COUNT) {
            snes_adapter->controllers[index].attached = false;
        }
    } else if (assignment->device == SNES_PORT_DEVICE_MOUSE) {
        snes_mouse_detach(&snes_adapter->mouse);
    }

    assignment->device = SNES_PORT_DEVICE_DETACHED;
    assignment->controller_index = SNES_PORT_DETACHED;
}

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio)
{
    snes_adapter->size = 0x00;
    snes_adapter->pio = pio;

    snes_controller_load_mappings();

    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        snes_adapter->port_bits[i] = 0xFFFF;
        snes_adapter->ports[i].device = SNES_PORT_DEVICE_DETACHED;
        snes_adapter->ports[i].controller_index = SNES_PORT_DETACHED;
    }

    for (uint8_t i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        snes_controller_init(&snes_adapter->controllers[i], i);
    }

    snes_mouse_init(&snes_adapter->mouse);
    snes_adapter->mouse.machine = pio->machine;
    snes_adapter_listen(snes_adapter);
    snes_adapter_set_mouse_port(snes_adapter, SNES_MOUSE_DEFAULT_PORT);

    for (uint8_t i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        if (snes_controller_available(i)) {
            printf("[SNES] Found \"%s\"\n", snes_controller_name(i));
            for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
                if (snes_adapter->ports[port].device == SNES_PORT_DEVICE_DETACHED) {
                    snes_adapter_set_controller_port(snes_adapter, i, port);
                    break;
                }
            }
        }
    }

    return 0;
}

void snes_adapter_attach(snes_adapter_t* snes_adapter, uint8_t index)
{
    snes_adapter_set_controller_port(snes_adapter, index, index);
    snes_adapter_listen(snes_adapter);
}

void snes_adapter_set_controller_port(snes_adapter_t *snes_adapter, uint8_t index, int port)
{
    if (index >= SNES_GAMEPAD_COUNT) {
        return;
    }

    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        if (snes_adapter->ports[i].device == SNES_PORT_DEVICE_CONTROLLER &&
            snes_adapter->ports[i].controller_index == index) {
            snes_adapter_detach_port(snes_adapter, i);
        }
    }

    if (port < 0 || port >= SNES_CONTROLLER_COUNT) {
        printf("[SNES] Detached \"%s\"\n", snes_controller_name(index));
        return;
    }

    snes_adapter_detach_port(snes_adapter, port);
    snes_adapter->ports[port].device = SNES_PORT_DEVICE_CONTROLLER;
    snes_adapter->ports[port].controller_index = index;
    snes_adapter->controllers[index].index = index;
    snes_adapter->controllers[index].attached = false;

    printf("[SNES] Attached \"%s\" to port %d\n", snes_controller_name(index), port);
}

void snes_adapter_set_mouse_port(snes_adapter_t *snes_adapter, int port)
{
    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        if (snes_adapter->ports[i].device == SNES_PORT_DEVICE_MOUSE) {
            snes_adapter_detach_port(snes_adapter, i);
        }
    }

    if (port < 0 || port >= SNES_CONTROLLER_COUNT) {
        printf("[SNES] Mouse detached\n");
        return;
    }

    snes_adapter_detach_port(snes_adapter, port);
    snes_adapter->mouse.attached = true;
    snes_adapter->ports[port].device = SNES_PORT_DEVICE_MOUSE;
    snes_adapter->ports[port].controller_index = SNES_PORT_DETACHED;

    printf("[SNES] Attached mouse to port %d\n", port);
}

int snes_adapter_get_controller_port(const snes_adapter_t *snes_adapter, uint8_t index)
{
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        if (snes_adapter->ports[port].device == SNES_PORT_DEVICE_CONTROLLER &&
            snes_adapter->ports[port].controller_index == index) {
            return port;
        }
    }

    return SNES_PORT_DETACHED;
}

int snes_adapter_get_mouse_port(const snes_adapter_t *snes_adapter)
{
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        if (snes_adapter->ports[port].device == SNES_PORT_DEVICE_MOUSE) {
            return port;
        }
    }

    return SNES_PORT_DETACHED;
}

void snes_adapter_reset_mouse_scale(snes_adapter_t *snes_adapter)
{
    snes_mouse_reset_scale(&snes_adapter->mouse);
}

void snes_adapter_detach(snes_adapter_t* snes_adapter)
{
    snes_mouse_detach(&snes_adapter->mouse);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}
