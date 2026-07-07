#include <stdint.h>
#include <stdio.h>

#include "utils/log.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes_adapter/controller.h"
#include "hw/userport/snes_adapter/mouse.h"
#include "hw/pio.h"
#include "hw/zeal.h"


static uint8_t snes_adapter_data_pin(uint8_t port)
{
    return port == 0 ? SNES_IO_DATA_1 : SNES_IO_DATA_2;
}

static bool snes_adapter_port_attached(const snes_adapter_t* snes_adapter, uint8_t port)
{
    return snes_adapter->ports[port].device != SNES_PORT_DEVICE_DETACHED;
}

static void snes_adapter_attach_available_controllers(snes_adapter_t* snes_adapter);

static const char* snes_adapter_controller_name(const snes_adapter_t* snes_adapter, uint8_t index)
{
    if (index == 0 && snes_adapter->virtual_controller_enabled &&
        !snes_controller_available(index)) {
        return "On-screen controller";
    }

    const char* name = snes_controller_name(index);
    return name != NULL ? name : "Unavailable controller";
}

static const char* snes_adapter_device_name(snes_port_device_t device)
{
    switch (device) {
        case SNES_PORT_DEVICE_CONTROLLER:
            return "ctrl";
        case SNES_PORT_DEVICE_MOUSE:
            return "mouse";
        case SNES_PORT_DEVICE_DETACHED:
        default:
            return "none";
    }
}

static void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit)
{
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        if (!snes_adapter_port_attached(snes_adapter, port)) {
            /* Simulate a pull-up resistor */
            pio_set_a_pin(pio, snes_adapter_data_pin(port), 1);
            continue;
        }

        snes_adapter->port_bits[port] >>= 1;
        snes_adapter->port_bits[port] |= 0x80000000;
        pio_set_a_pin(pio, snes_adapter_data_pin(port), snes_adapter->port_bits[port] & 0x01);
    }

    v_log_printf(3, "[SNES] PIO clock port1=%s/%d/%08x port2=%s/%d/%08x\n",
        snes_adapter_device_name(snes_adapter->ports[0].device),
        snes_adapter->ports[0].controller_index,
        snes_adapter->port_bits[0],
        snes_adapter_device_name(snes_adapter->ports[1].device),
        snes_adapter->ports[1].controller_index,
        snes_adapter->port_bits[1]);
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
                {
                    int index = snes_adapter->ports[port].controller_index;
                    uint16_t bits = snes_controller_latch(&snes_adapter->controllers[index]);
                    if (index == 0 && snes_adapter->virtual_controller_enabled) {
                        bits &= snes_adapter->virtual_controller_bits;
                    }
                    snes_adapter->port_bits[port] = bits | 0xFFFF0000;
                }
                break;
            case SNES_PORT_DEVICE_DETACHED:
            default:
                continue;
        }

        pio_set_a_pin(pio, snes_adapter_data_pin(port), snes_adapter->port_bits[port] & 0x01);
    }

    v_log_printf(3, "[SNES] PIO latch port1=%s/%d/%04x port2=%s/%d/%04x\n",
        snes_adapter_device_name(snes_adapter->ports[0].device),
        snes_adapter->ports[0].controller_index,
        snes_adapter->port_bits[0],
        snes_adapter_device_name(snes_adapter->ports[1].device),
        snes_adapter->ports[1].controller_index,
        snes_adapter->port_bits[1]);

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
            snes_adapter->controllers[index].port = SNES_PORT_DETACHED;
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
    snes_adapter->virtual_controller_bits = 0xFFFF;
    snes_adapter->virtual_controller_enabled = false;

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
#ifdef CONFIG_AUTO_ATTACH_SNES_MOUSE
    snes_adapter_set_mouse_port(snes_adapter, SNES_MOUSE_DEFAULT_PORT);
#endif

    snes_adapter_attach_available_controllers(snes_adapter);

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
        printf("[SNES] Detached \"%s\"\n", snes_adapter_controller_name(snes_adapter, index));
        return;
    }

    snes_adapter_detach_port(snes_adapter, port);
    snes_adapter->ports[port].device = SNES_PORT_DEVICE_CONTROLLER;
    snes_adapter->ports[port].controller_index = index;
    snes_adapter->controllers[index].index = index;
    snes_adapter->controllers[index].port = port;
    snes_adapter->controllers[index].attached = snes_controller_available(index);

    printf("[SNES] Attached \"%s\" to port %d\n",
        snes_adapter_controller_name(snes_adapter, index), port);
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

void snes_adapter_update(snes_adapter_t *snes_adapter)
{
    for (uint8_t i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        bool available = snes_controller_available(i);
        bool needed_for_virtual_controller = i == 0 && snes_adapter->virtual_controller_enabled;

        if (!available && !needed_for_virtual_controller &&
            snes_adapter_get_controller_port(snes_adapter, i) != SNES_PORT_DETACHED) {
            snes_adapter_set_controller_port(snes_adapter, i, SNES_PORT_DETACHED);
        }
        snes_adapter->controllers[i].attached = available;
    }

    snes_adapter_attach_available_controllers(snes_adapter);
    snes_adapter->mouse.attached = snes_adapter_get_mouse_port(snes_adapter) != SNES_PORT_DETACHED;
    snes_mouse_update(&snes_adapter->mouse);
}

static void snes_adapter_attach_available_controllers(snes_adapter_t* snes_adapter)
{
    for (uint8_t i = 0; i < SNES_GAMEPAD_COUNT; i++) {
        bool available = snes_controller_available(i);
        bool needed_for_virtual_controller = i == 0 && snes_adapter->virtual_controller_enabled;
        if ((!available && !needed_for_virtual_controller) ||
            snes_adapter_get_controller_port(snes_adapter, i) != SNES_PORT_DETACHED) {
            continue;
        }

        int port = SNES_PORT_DETACHED;
        if (i == 0) {
            port = 0;
        } else {
            for (uint8_t candidate = 0; candidate < SNES_CONTROLLER_COUNT; candidate++) {
                if (snes_adapter->ports[candidate].device == SNES_PORT_DEVICE_DETACHED) {
                    port = candidate;
                    break;
                }
            }
            if (port == SNES_PORT_DETACHED &&
                snes_adapter->ports[1].device == SNES_PORT_DEVICE_MOUSE) {
                port = 1;
            }
        }

        if (port != SNES_PORT_DETACHED) {
            if (available) {
                printf("[SNES] Found \"%s\" (%d axes)\n",
                    snes_adapter_controller_name(snes_adapter, i),
                    snes_controller_axis_count(i));
            }
            snes_adapter_set_controller_port(snes_adapter, i, port);
        }
    }
}

void snes_adapter_set_virtual_button(snes_adapter_t *snes_adapter, uint8_t button, bool pressed)
{
    if (button > SNES_BTN_R) {
        return;
    }

    snes_adapter->virtual_controller_enabled = true;
    if (pressed) {
        snes_adapter->virtual_controller_bits &= (uint16_t)~(1u << button);
    } else {
        snes_adapter->virtual_controller_bits |= (uint16_t)(1u << button);
    }
    snes_adapter_attach_available_controllers(snes_adapter);
}

void snes_adapter_clear_virtual_buttons(snes_adapter_t *snes_adapter)
{
    snes_adapter->virtual_controller_enabled = true;
    snes_adapter->virtual_controller_bits = 0xFFFF;
    snes_adapter_attach_available_controllers(snes_adapter);
}

void snes_adapter_reset_mouse_scale(snes_adapter_t *snes_adapter)
{
    snes_mouse_reset_scale(&snes_adapter->mouse);
}

void snes_adapter_detach(snes_adapter_t* snes_adapter)
{
    snes_mouse_detach(&snes_adapter->mouse);
    snes_controller_deinit();
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}
