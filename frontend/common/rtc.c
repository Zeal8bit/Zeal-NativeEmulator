/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <time.h>
#include "frontend/common/media.h"
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


static time_t ds1307_to_time(const uint8_t *ram) {
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


static void rtc_read_time(void* userdata, int64_t offset, uint8_t* ram)
{
    (void)userdata;
    /* Take into account the potential time difference programmed */
    const time_t now = time(NULL) + offset;
    const struct tm *tm = localtime(&now);
    ram[DS1307_SEC_REG]  = dec_to_bcd(tm->tm_sec);        // Seconds
    ram[DS1307_MIN_REG]  = dec_to_bcd(tm->tm_min);        // Minutes
    ram[DS1307_HOUR_REG] = dec_to_bcd(tm->tm_hour);       // Hours
    ram[DS1307_DAY_REG]  = dec_to_bcd(tm->tm_wday + 1);   // Day of week (1-7)
    ram[DS1307_DAT_REG]  = dec_to_bcd(tm->tm_mday);       // Day of month
    ram[DS1307_MON_REG]  = dec_to_bcd(tm->tm_mon + 1);    // Month (1-12)
    ram[DS1307_YEAR_REG] = dec_to_bcd(tm->tm_year % 100); // Year (last two digits)
}


static int64_t rtc_adjust_time(void* userdata, const uint8_t* ram)
{
    (void)userdata;
    const time_t base = ds1307_to_time(ram);
    const time_t now = time(NULL);
    return base - now;
}

void frontend_rtc_bind(ds1307_t* rtc)
{
    rtc->read_time = rtc_read_time;
    rtc->adjust_time = rtc_adjust_time;
}
