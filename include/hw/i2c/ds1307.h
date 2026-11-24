/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hw/i2c/i2c_device.h"

#define DS1307_ADDR  0x68
#define DS1307_LEN   0x40

/* Fill the DS1307 time registers for "now + offset". */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ds1307_read_time_t)(void* userdata, int64_t offset, uint8_t* registers);
/* Return the offset to add to "now" so the registers read back as given. */
typedef int64_t (*ds1307_adjust_time_t)(void* userdata, const uint8_t* registers);

typedef struct ds1307_t {
    struct i2c_device_t parent;
    /* Time base in seconds */
    int64_t time_diff;
    void* userdata;
    ds1307_read_time_t   read_time;
    ds1307_adjust_time_t adjust_time;
    /* Current register */
    uint8_t  reg;
    /* Bytes cycles received */
    int      count;
    uint8_t  ram[DS1307_LEN];
    /* Mark whether any time-related register changed */
    bool     changed;
} ds1307_t;


int ds1307_init(ds1307_t* rtc);

/**
 * @brief Bind the frontend's clock callbacks; pass NULL callbacks to detach.
 */
void ds1307_bind(ds1307_t* rtc, ds1307_read_time_t read_time,
                 ds1307_adjust_time_t adjust_time, void* userdata);

#ifdef __cplusplus
} /* extern "C" */
#endif
