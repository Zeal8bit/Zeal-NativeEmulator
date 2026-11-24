/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
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


/* Called at each I2C STOP that ends a page write, with the offset of the
 * complete AT24C512_PAGE-byte page. `userdata` is the frontend's image
 * handle (see at24c512_set_persist). */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*at24c512_persist_t)(void* userdata, uint32_t offset, const uint8_t* page);


typedef struct at24c512_t {
    struct i2c_device_t parent;
    uint8_t     data[AT24C512_SIZE];
    uint16_t    address;
    int         sector_written;
    int         count;
    bool        writing;
    void*               userdata;
    at24c512_persist_t  persist;
} at24c512_t;


/**
 * @brief Initialize the emulated EEPROM.
 */
int at24c512_init(at24c512_t* eeprom);

/**
 * @brief Bind the frontend's persistence sink; pass NULL callbacks to detach.
 */
void at24c512_set_persist(at24c512_t* eeprom, at24c512_persist_t persist, void* userdata);

#ifdef __cplusplus
} /* extern "C" */
#endif
