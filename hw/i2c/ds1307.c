/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include "hw/i2c/ds1307.h"

#define DS1307_SEC_REG  0x0
#define DS1307_MIN_REG  0x1
#define DS1307_HOUR_REG 0x2
#define DS1307_DAY_REG  0x3
#define DS1307_DAT_REG  0x4
#define DS1307_MON_REG  0x5
#define DS1307_YEAR_REG 0x6
#define DS1307_CTRL_REG 0x7


static inline uint8_t dec_to_bcd(int val) {
    return ((val / 10) << 4) | (val % 10);
}


static int bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}


static time_t ds1307_to_time(uint8_t *ram) {
    struct tm tm_time;

    tm_time.tm_sec  = bcd_to_bin(ram[DS1307_SEC_REG] & 0x7F);     // Seconds (mask out CH bit)
    tm_time.tm_min  = bcd_to_bin(ram[DS1307_MIN_REG] & 0x7F);     // Minutes
    tm_time.tm_hour = bcd_to_bin(ram[DS1307_HOUR_REG] & 0x3F);    // Hours (24-hour mode)
    tm_time.tm_mday = bcd_to_bin(ram[DS1307_DAT_REG] & 0x3F);     // Day of the month
    tm_time.tm_mon  = bcd_to_bin(ram[DS1307_MON_REG] & 0x1F) - 1; // Month (0-11)
    tm_time.tm_year = bcd_to_bin(ram[DS1307_YEAR_REG]) + 100;     // Year since 1900 (2000 + x)

    /* Ignore weekday since it is not used by time_t */
    tm_time.tm_wday = 0;
    tm_time.tm_yday = 0;
    /* Ignore daylight saving time */
    tm_time.tm_isdst = -1;

    return mktime(&tm_time);
}


static void get_current_time_bcd(ds1307_t* rtc)
{
    /* Take into account the potential time difference programmed */
    const time_t now = time(NULL) + rtc->time_diff;
    const struct tm *tm = localtime(&now);
    rtc->ram[DS1307_SEC_REG]  = dec_to_bcd(tm->tm_sec);        // Seconds
    rtc->ram[DS1307_MIN_REG]  = dec_to_bcd(tm->tm_min);        // Minutes
    rtc->ram[DS1307_HOUR_REG] = dec_to_bcd(tm->tm_hour);       // Hours
    rtc->ram[DS1307_DAY_REG]  = dec_to_bcd(tm->tm_wday + 1);   // Day of week (1-7)
    rtc->ram[DS1307_DAT_REG]  = dec_to_bcd(tm->tm_mday);       // Day of month
    rtc->ram[DS1307_MON_REG]  = dec_to_bcd(tm->tm_mon + 1);    // Month (1-12)
    rtc->ram[DS1307_YEAR_REG] = dec_to_bcd(tm->tm_year % 100); // Year (last two digits)
}


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
    get_current_time_bcd(rtc);
}


static void ds1307_stop(i2c_device_t* dev)
{
    ds1307_t* rtc = (ds1307_t*) dev;
    rtc->reg = 0;
    if (rtc->changed) {
        /* Store the difference with now */
        const time_t base = ds1307_to_time(rtc->ram);
        const time_t now = time(NULL);
        rtc->time_diff = base - now;
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
