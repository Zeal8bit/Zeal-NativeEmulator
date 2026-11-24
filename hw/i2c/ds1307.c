/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "hw/i2c/ds1307.h"

#define DS1307_SEC_REG  0x0
#define DS1307_MIN_REG  0x1
#define DS1307_HOUR_REG 0x2
#define DS1307_DAY_REG  0x3
#define DS1307_DAT_REG  0x4
#define DS1307_MON_REG  0x5
#define DS1307_YEAR_REG 0x6
#define DS1307_CTRL_REG 0x7


static uint8_t ds1307_read(i2c_device_t* dev)
{
    ds1307_t* rtc = (ds1307_t*) dev;
    const uint8_t data = rtc->ram[rtc->reg];
    rtc->reg = (rtc->reg + 1) % DS1307_LEN;
    return data;
}


static void ds1307_write(i2c_device_t* dev, uint8_t data)
{
    ds1307_t* rtc = (ds1307_t*) dev;
    if (rtc->count == 0) {
        rtc->reg = data;
    } else {
        if (rtc->reg <= DS1307_YEAR_REG) {
            rtc->changed = true;
        }
        rtc->ram[rtc->reg] = data;
        rtc->reg++;
    }
    rtc->count++;
}


static void ds1307_start(i2c_device_t* dev)
{
    ds1307_t* rtc = (ds1307_t*) dev;
    rtc->count = 0;
    rtc->changed = false;
    if (rtc->read_time) {
        rtc->read_time(rtc->userdata, rtc->time_diff, rtc->ram);
    }
}


static void ds1307_stop(i2c_device_t* dev)
{
    ds1307_t* rtc = (ds1307_t*) dev;
    rtc->reg = 0;
    if (rtc->changed) {
        /* Store the difference with now */
        if (rtc->adjust_time) {
            rtc->time_diff = rtc->adjust_time(rtc->userdata, rtc->ram);
        }
        rtc->changed = false;
    }
}


int ds1307_init(ds1307_t* rtc)
{
    if (rtc == NULL) {
        return 1;
    }

    rtc->parent.address = DS1307_ADDR;
    rtc->parent.start = ds1307_start;
    rtc->parent.read  = ds1307_read;
    rtc->parent.write = ds1307_write;
    rtc->parent.stop  = ds1307_stop;
    rtc->reg          = 0;
    rtc->time_diff    = 0;
    rtc->ram[DS1307_CTRL_REG] = 0;

    return 0;
}

void ds1307_bind(ds1307_t* rtc, ds1307_read_time_t read_time,
                 ds1307_adjust_time_t adjust_time, void* userdata)
{
    if (rtc == NULL) {
        return;
    }
    rtc->read_time = read_time;
    rtc->adjust_time = adjust_time;
    rtc->userdata = userdata;
}
