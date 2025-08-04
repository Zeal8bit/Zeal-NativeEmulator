/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <string.h>

#include "utils/log.h"
#include "utils/helpers.h"
#include "hw/pio.h"
#include "hw/i2c.h"

// #define debug log_printf
#define debug(...)

#define ACK     0
#define NACK    1


static inline bool i2c_is_read(i2c_t* ctrl)
{
    return (ctrl->dev_addr & 1) == 1;
}


static inline uint_fast8_t i2c_dev_addr(i2c_t* ctrl)
{
    return (ctrl->dev_addr >> 1);
}

static inline i2c_device_t* i2c_cur_device(i2c_t* ctrl)
{
    return ctrl->devices[i2c_dev_addr(ctrl)];
}


static int process_byte(i2c_t* i2c, uint8_t data, uint8_t* next_byte)
{
    i2c_device_t* dev = NULL;

    switch(i2c->st) {
        case I2C_IDLE:
            log_err_printf("[I2C] Warning: received a byte without start!\n");
            return NACK;
        case I2C_START_RCVED:
        case I2C_RESTART_RCVED:
            i2c->st++;   // Make the assumption that *ADDR is the next state;
            i2c->dev_addr = data;
            dev = i2c_cur_device(i2c);
            i2c->output = i2c_is_read(i2c);

            if (dev == NULL) {
                log_printf("[I2C] Warning: no device found at address 0x%x\n", data);
                return NACK;
            } else if (dev->start) {
                dev->start(dev);
            }
            if (i2c->output && dev->read) {
                /* If the transaction is a read, get the next byte right now */
                *next_byte = dev->read(dev);
            }

            /* Send ACK (0) if dev is not NULL */
            return ACK;

        case I2C_ADDR_RCVED:
        case I2C_READDR_RCVED:
            /* Send the byte to the device */
            dev = i2c_cur_device(i2c);
            if (dev != NULL) {
                if (i2c_is_read(i2c)) {
                    /* Get the next byte to send */
                    *next_byte = dev->read(dev);
                } else {
                    dev->write(dev, data);
                }
            }
            return ACK;
    }

    return NACK;
}


/**
 * @brief Function called when the master is ready a byte, so we need to output
 * some data on the bus.
 */
static void output_mode(pio_t* pio, i2c_t* i2c, int scl)
{
    if (scl == 0) {
        /* Shift byte out */
        const int next_bit = (i2c->cur_byte & 0x80) ? 1 : 0;
        pio_set_b_pin(pio, IO_I2C_SDA_IN_PIN, next_bit);
        i2c->cur_byte = i2c->cur_byte << 1;
        i2c->cur_bit++;
    } else if (i2c->cur_bit == 8) {
        /* SCL is high, check if we finished shifting the byte and prepare the next one */
        i2c->cur_bit = 0;
        (void) process_byte(i2c, 0, &i2c->cur_byte);
        i2c->has_reply = true;
    }
}


static void input_mode(pio_t* pio, i2c_t* i2c, int scl)
{
    /* Input mode, waiting for data, SCL must be high */
    if (scl != 0) {
        /* Read the SDA line */
        const int sda = pio_get_b_pin(pio, IO_I2C_SDA_OUT_PIN);
        i2c->cur_byte = (i2c->cur_byte << 1) | (sda & 1);
        i2c->cur_bit++;
        if (i2c->cur_bit == 8) {
            i2c->cur_bit = 0;
            uint8_t next_byte = 0;
            int ack = process_byte(i2c, i2c->cur_byte, &next_byte);
            i2c->cur_byte = next_byte;
            pio_set_b_pin(pio, IO_I2C_SDA_IN_PIN, ack);
            i2c->has_reply = true;
        }
    }
}


static void write_scl(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    i2c_t* i2c = (i2c_t*) arg;

    if (read || !transition) {
        return;
    }

    debug("[I2C] SCL: %d, SDA: %d, has_reply: %d, output: %d, cur_byte %x, cur_bit: %d\n",
        bit, pio_get_b_pin(pio, IO_I2C_SDA_OUT_PIN), i2c->has_reply, i2c->output, i2c->cur_byte, i2c->cur_bit);

    /* Ignore the coming cycle if it is an ACK/NACK */
    if (i2c->has_reply) {
        if (bit == 0)
            /* Setting up the reply */
            return;
        /* In theory, we should check the reply from the master */
        i2c->has_reply = false;
        return;
    }

    if (i2c->output) {
        output_mode(pio, i2c, bit);
    } else {
        input_mode(pio, i2c, bit);
    }

    (void) pin;
}


static void i2c_clear_data(i2c_t* i2c)
{
    i2c->cur_bit  = 0;
    i2c->cur_byte = 0;
    i2c->dev_addr = 0;
    i2c->output = false;
    i2c->has_reply = false;
}


static void write_sda(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    i2c_t* i2c = (i2c_t*) arg;
    /* Let's only use this function to check for a START or a STOP, else ignore */
    const int scl = pio_get_b_pin(pio, IO_I2C_SCL_OUT_PIN);

    if (read || !transition || scl == 0) {
        return;
    }

    /* START: SDA going low while SCL is high */
    if (bit == 0) {
        if (i2c->st == I2C_IDLE) {
            i2c->st = I2C_START_RCVED;
        } else if (i2c->st == I2C_ADDR_RCVED) {
            i2c->st = I2C_RESTART_RCVED;
            i2c_clear_data(i2c);
        } else {
            log_printf("[I2C] Warning: invalid I2C protocol detected!\n");
            i2c->st = I2C_START_RCVED;
            return;
        }
    }
    /* STOP: SDA going high while SCL is high */
    else {
        i2c_device_t* dev = i2c_cur_device(i2c);
        if (dev != NULL && dev->stop != NULL) {
            dev->stop(dev);
        }
        i2c->st = I2C_IDLE;
        i2c_clear_data(i2c);
    }

    (void) pin;
}


int i2c_init(i2c_t* bus, pio_t* pio)
{
    /* TODO: implement custom argument to the PIO callbacks */
    memset(bus, 0, sizeof(i2c_t));

    pio_set_b_pin(pio, IO_I2C_SCL_OUT_PIN, 1);
    pio_set_b_pin(pio, IO_I2C_SDA_OUT_PIN, 1);

    pio_listen_b_pin(pio, IO_I2C_SCL_OUT_PIN, write_scl, bus);
    pio_listen_b_pin(pio, IO_I2C_SDA_OUT_PIN, write_sda, bus);

    return 0;
}


int i2c_connect(i2c_t* bus, i2c_device_t* device)
{
    if (device == NULL) {
        return 1;
    }

    if (device->address >= 0x80) {
        log_err_printf("[I2C] Cannot connect device 0x%x: invalid address\n", device->address);
        return 1;
    }

    if (bus->devices[device->address] != NULL) {
        log_err_printf("[I2C] Warning: two devices connected to address 0x%x\n", device->address);
        return 1;
    }

    bus->devices[device->address] = device;
    return 0;
}
