#pragma once

#include <stdint.h>
#include <stddef.h>
#include "hw/device.h"

#define MODE_OUTPUT  0
#define MODE_INPUT   1
#define MODE_BIDIR   2
#define MODE_BITCTRL 3

#define PIO_PIN_COUNT 8

#define DIR_OUTPUT 0
#define DIR_INPUT  1

#define BIT(val, n) (((val) >> (n)) & 1)

typedef void (*pio_listener_callback)(uint8_t, uint8_t, uint8_t, uint8_t);
typedef void (*pio_listener_change_callback)(uint8_t, uint8_t);

typedef struct {
    uint8_t state;
    pio_listener_change_callback callback;
} listener_change_t;

typedef struct {
    char port; // a,b
    uint8_t mode;
    uint8_t state;
    uint8_t dir;
    uint8_t int_vector;
    uint8_t int_enable;
    uint8_t int_mask;
    uint8_t and_op;
    uint8_t active_high;
    uint8_t mask_follows;
    uint8_t dir_follows;

    // TODO: listeners is an array of callbacks
    pio_listener_callback listeners[PIO_PIN_COUNT];
    listener_change_t listeners_change[PIO_PIN_COUNT];
} port_t;

typedef struct {
    // device_t
    device_t parent;
    size_t size; // in bytes
    // pio specific
    port_t port_a;
    port_t port_b;
} pio_t;


int pio_init(void *machine, pio_t *pio);
void pio_set_pin(port_t *port, uint8_t pin, uint8_t value);
uint8_t pio_get_pin(port_t *port, uint8_t pin);
void pio_listen_pin(port_t *port, uint8_t pin, pio_listener_callback cb);

void pio_set_a_pin(pio_t* pio, uint8_t pin, uint8_t value);
uint8_t pio_get_a_pin(pio_t* pio, uint8_t pin);
void pio_set_b_pin(pio_t* pio, uint8_t pin, uint8_t value);
uint8_t pio_get_b_pin(pio_t* pio, uint8_t pin);


void pio_listen_a_pin(pio_t* pio, uint8_t pin, pio_listener_callback cb);
void pio_unlisten_a_pin(pio_t* pio, uint8_t pin);

void pio_listen_a_pin_change(pio_t* pio, uint8_t pin, uint8_t state, pio_listener_change_callback cb);
void pio_unlisten_a_pin_change(pio_t* pio, uint8_t pin);

void pio_listen_b_pin(pio_t* pio, uint8_t pin, pio_listener_callback cb);
void pio_unlisten_b_pin(pio_t* pio, uint8_t pin);

void pio_listen_b_pin_change(pio_t* pio, uint8_t pin, uint8_t state, pio_listener_change_callback cb);
void pio_unlisten_b_pin_change(pio_t* pio, uint8_t pin);
