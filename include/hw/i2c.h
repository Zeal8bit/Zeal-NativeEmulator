#pragma once

#include <stdint.h>
#include <stddef.h>

#include "hw/pio.h"
#include "i2c/i2c_device.h"


#define IO_I2C_SDA_IN_PIN   2
#define IO_I2C_SCL_OUT_PIN  1
#define IO_I2C_SDA_OUT_PIN  0
#define I2C_MAX_DEV         128


typedef enum {
    I2C_IDLE = 0,
    I2C_START_RCVED,
    I2C_ADDR_RCVED,
    I2C_RESTART_RCVED,
    I2C_READDR_RCVED,
} i2c_state_t;


typedef struct {
    int cur_bit;
    uint8_t cur_byte;
    uint8_t dev_addr;
    /* Shift register for outputting data */
    bool output;
    /* True if we are sending (or receiving ACK bit)*/
    bool has_reply;
    i2c_state_t st;
    i2c_device_t* devices[I2C_MAX_DEV];
} i2c_t;


/**
 * @brief Initialize the I2C bus.
 *
 * @returns 0 on success
 */
int i2c_init(i2c_t* bus, pio_t* pio);


/**
 * @brief Connect a device on the I2C bus.
 *
 * @returns 0 on success
 */
int i2c_connect(i2c_t* bus, i2c_device_t* device);
