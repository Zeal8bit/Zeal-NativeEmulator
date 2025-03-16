#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct i2c_device_t i2c_device_t;

struct i2c_device_t {
    /* 7-bit address of the device */
    uint8_t address;
    void    (*start)(i2c_device_t* dev);
    uint8_t (*read)(i2c_device_t* dev);
    void    (*write)(i2c_device_t* dev, uint8_t data);
    void    (*stop)(i2c_device_t* dev);
};
