#include <stdint.h>
#include <stdlib.h>

#include "hw/zeal.h"
#include "hw/pio.h"
#include "hw/z80.h"
#include "hw/device.h"

static void* zeal;

static uint8_t io_read(device_t* dev, uint32_t addr)
{
    pio_t *pio = (pio_t *)dev;
    port_t* hw_port = (addr & 1) == 0 ? &pio->port_a : &pio->port_b;
    uint8_t ctrl    = (addr & 2) != 0;
    // uint8_t data    = !ctrl;

    if (ctrl) {
        return 0x43;
    }
    return hw_port->state;
}

static void io_write(device_t* dev, uint32_t addr, uint8_t value)
{
    pio_t *pio = (pio_t *)dev;
    port_t* hw_port = (addr & 1) == 0 ? &pio->port_a : &pio->port_b;
    uint8_t ctrl    = (addr & 2) != 0;
    uint8_t data    = !ctrl;

    if (ctrl && hw_port->dir_follows) {
        hw_port->dir_follows = 0;
        hw_port->dir         = value & 0xff;
    } else if (ctrl && hw_port->mask_follows) {
        hw_port->mask_follows = 0;
        /* Save the mask */
        hw_port->int_mask = value & 0xff;
    } else if (ctrl && (value & 0xf) == 0xf) {
        /* Word Set */
        /* Upper two bits define the mode to operate in */
        hw_port->mode        = (value >> 6) & 0x3;
        hw_port->dir_follows = (hw_port->mode == MODE_BITCTRL);
    } else if (ctrl && (value & 0xf) == 7) {
        /* Interrupt Control Word */
        hw_port->mask_follows = BIT(value, 4) == 1;
        hw_port->active_high  = BIT(value, 5) == 1;
        hw_port->and_op       = BIT(value, 6) == 1;
        hw_port->int_enable   = BIT(value, 7) == 1;
        /* As stated in the PIO User manual, if mask follows is set, the interrupt requests are
         * reset, in our case, let's just reset the mask we have */
        if (hw_port->mask_follows) {
            hw_port->int_mask = 0xff;
        }
    } else if (ctrl && (value & 0xf) == 3) {
        /* Disable the interrupt flip-flop */
        hw_port->int_enable = BIT(value, 7) == 1;
    } else if (ctrl && (value & 1) == 0) {
        /* When the LSB is 0, it means the interrupt vector is being programmed */
        hw_port->int_vector = value & 0xff;
    } else if (data && hw_port->mode != MODE_INPUT) {
        /* Backup the state to check for transitions on pins */
        uint8_t former_state = hw_port->state;
        if (hw_port->mode == MODE_BIDIR || hw_port->mode == MODE_OUTPUT) {
            hw_port->state = value & 0xff;
        } else {
            /* Only modify the state of output pins, input are not modified.
             * Input pins are set to 1, so invert the direction register. */
            uint8_t new_out_val = value & (~hw_port->dir & 0xff);
            /* Clear the output pins value from the state and OR it with the new value */
            hw_port->state = (hw_port->state & hw_port->dir) | new_out_val;
        }

        for (uint8_t pin = 0; pin < 8; pin++) {
            pio_listener_callback listener    = hw_port->listeners[pin];
            listener_change_t* listener_change = &hw_port->listeners_change[pin];
            uint8_t bit                        = BIT(hw_port->state, pin);
            uint8_t formerBit                  = BIT(former_state, pin);
            if (listener != NULL && BIT(hw_port->dir, pin) == DIR_OUTPUT) {
                uint8_t transition = formerBit != bit;
                /* Parameters(read, pin, value, transition) */
                listener(0, pin, bit, transition);
            }
            if (listener_change->callback != NULL && formerBit != bit && listener_change->state == bit) {
                /* Parameters(pin, value) */
                listener_change->callback(pin, bit);
            }
        }
    }
}


int pio_init(void* machine, pio_t* pio)
{
    pio->port_a = (port_t){
        .port         = 'a',
        .mode         = MODE_OUTPUT,
        .state        = 0xf0,
        .dir          = 0xff,
        .int_vector   = 0,
        .int_enable   = 0,
        .int_mask     = 0,
        .and_op       = 1,
        .active_high  = 0,
        .mask_follows = 0,
        .dir_follows  = 0,
    };

    pio->port_b = (port_t){
        .port         = 'b',
        .mode         = MODE_OUTPUT,
        .state        = 0xf0,
        .dir          = 0xff,
        .int_vector   = 0,
        .int_enable   = 0,
        .int_mask     = 0,
        .and_op       = 1,
        .active_high  = 0,
        .mask_follows = 0,
        .dir_follows  = 0,
    };

    for (uint8_t i = 0; i < PIO_PIN_COUNT; i++) {
        pio->port_a.listeners[i]        = NULL;
        pio->port_b.listeners[i]        = NULL;
        pio->port_a.listeners_change[i].callback = NULL;
        pio->port_a.listeners_change[i].state = 0;
        pio->port_b.listeners_change[i].callback = NULL;
        pio->port_b.listeners_change[i].state = 0;
    }

    zeal = machine;

    pio->size = 0x10;

    device_init_io(DEVICE(pio), "pio_dev", io_read, io_write, pio->size);
    return 0;
}

/**
 * Check if the given pin (with the given new value) should trigger an interrupt, when mode is BITCTRL
 */
uint8_t pio_check_bitctrl_interrupt(port_t* port, uint8_t pin, uint8_t value)
{
    uint8_t activemask = port->active_high ? 0xff : 0;

    /* Check that BITCTRL mode is ON */
    return port->mode == MODE_BITCTRL &&
           /* Check if the pin is monitored */
           (port->int_mask & (1 << pin)) == 0 &&
           /* Check if the active state is not the same as the value */
           ((activemask & 1) == value) &&
           /* If the operation is an OR, we can already return true, else, the
            * port state must be either all 0 (if active low) or 0xff (active high)
            */
           (!port->and_op || (port->state == activemask));
}

/**
 * Set the pin of a port to a certain value: 0 or 1.
 */
void pio_set_pin(port_t* port, uint8_t pin, uint8_t value)
{
    uint8_t previous_state = port->state;
    if (value == 0) {
        port->state &= (~(1 << pin)) & 0xff;
    } else {
        port->state |= (1 << pin);
    }
    /* Check if the bit actually changed */
    uint8_t changed = (previous_state ^ port->state) != 0;

    /* Check if an interrupt need to be generated */
    if (port->int_enable && changed && port->mode != MODE_OUTPUT &&
        (port->mode != MODE_BITCTRL || pio_check_bitctrl_interrupt(port, pin, value))) {
        zeal_t *machine = (zeal_t*)zeal;
        z80_gen_int(&machine->cpu, port->int_vector);
    }
}

/**
 * Get the pin of a port.
 * Returns 0 or 1.
 */
uint8_t pio_get_pin(port_t* port, uint8_t pin)
{
    return BIT(port->state, pin);
}

void pio_set_a_pin(pio_t *pio, uint8_t pin, uint8_t value)
{
    pio_set_pin(&pio->port_a, pin, value);
}

uint8_t pio_get_a_pin(pio_t *pio, uint8_t pin)
{
    return pio_get_pin(&pio->port_a, pin);
}

void pio_set_b_pin(pio_t *pio, uint8_t pin, uint8_t value)
{
    pio_set_pin(&pio->port_b, pin, value);
}

uint8_t pio_get_b_pin(pio_t *pio, uint8_t pin)
{
    return pio_get_pin(&pio->port_b, pin);
}

void pio_listen_pin(port_t* port, uint8_t pin, pio_listener_callback cb)
{
    if (/*pin < 0 || */ pin > 7 || (cb != NULL && port->listeners[pin])) {
        return;
    }
    port->listeners[pin] = cb;
}

void pio_listen_pin_change(port_t* port, uint8_t pin, uint8_t state, pio_listener_change_callback cb)
{
    if (/*pin < 0 || */ pin > 7 || (cb != NULL && port->listeners[pin])) {
        return;
    }
    port->listeners_change[pin].callback = cb;
    port->listeners_change[pin].state    = state;
}

void pio_listen_a_pin(pio_t *pio, uint8_t pin, pio_listener_callback cb)
{
    pio_listen_pin(&pio->port_a, pin, cb);
}

void pio_listen_b_pin(pio_t *pio, uint8_t pin, pio_listener_callback cb)
{
    pio_listen_pin(&pio->port_b, pin, cb);
}

void pio_listen_a_pin_change(pio_t *pio, uint8_t pin, uint8_t state, pio_listener_change_callback cb)
{
    pio_listen_pin_change(&pio->port_a, pin, state, cb);
}

void pio_listen_b_pin_change(pio_t *pio, uint8_t pin, uint8_t state, pio_listener_change_callback cb)
{
    pio_listen_pin_change(&pio->port_b, pin, state, cb);
}

void pio_unlisten_pin(port_t* port, uint8_t pin)
{
    port->listeners_change[pin].callback = NULL;
    port->listeners_change[pin].state = 0;
}

void pio_unlisten_a_pin(pio_t *pio, uint8_t pin)
{
    pio_listen_pin(&pio->port_a, pin, NULL);
}

void pio_unlisten_b_pin(pio_t *pio, uint8_t pin)
{
    pio_listen_pin(&pio->port_b, pin, NULL);
}

void pio_unlisten_a_pin_change(pio_t *pio, uint8_t pin)
{
    pio_listen_pin_change(&pio->port_a, pin, 0, NULL);
}

void pio_unlisten_b_pin_change(pio_t *pio, uint8_t pin)
{
    pio_listen_pin_change(&pio->port_b, pin, 0, NULL);
}
