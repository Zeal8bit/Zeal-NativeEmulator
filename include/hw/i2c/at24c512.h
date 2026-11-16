/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hw/i2c/i2c_device.h"

#define AT24C512_ADDR    0x50
#define AT24C512_SIZE    0x10000  // 64KB
#define AT24C512_PAGE    128      // Page size: 128 bytes


typedef struct at24c512_t {
    struct i2c_device_t parent;
    uint8_t     data[AT24C512_SIZE];
    uint16_t    address;
    int         sector_written;
    int         count;
    bool        writing;
    /* Borrowed synchronous sink: one AT24C512_PAGE buffer at I2C STOP. */
    void* userdata;
    void (*persist)(void*, uint32_t offset, const uint8_t* page);
} at24c512_t;


/**
 * @brief Initialize the emulated EEPROM; the frontend supplies its image and persistence sink.
 */
int at24c512_init(at24c512_t* eeprom);
