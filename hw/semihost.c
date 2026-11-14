/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "hw/semihost.h"
#include "hw/host.h"

static void semihost_counter_reset_stats(semihost_counter_t* counter)
{
    counter->start_cyc = 0;
    counter->last_split_cyc = 0;
    counter->min_us = 0;
    counter->max_us = 0;
    counter->total_interval_us = 0;
    counter->last_total_us = 0;
    counter->sample_count = 0;
    counter->running = false;
}

static int semihost_read_byte(semihost_t* dev)
{
    const hw_host_t* host = dev->cpu->mmu.parent.host;
    return host && host->read_byte ? host->read_byte(host->userdata) : -1;
}

static uint8_t semihost_op_read_char(semihost_t* dev)
{
    int c = semihost_read_byte(dev);
    if (c >= 0) {
        return c;
    }
    return 0xFF;
}

static uint16_t semihost_op_read_int(semihost_t* dev)
{
    char buffer[6]; /* Max 5 digits for 16-bit unsigned (65535) */
    int bytes_read = 0;
    uint8_t c;
    uint16_t value = 0;

    /* Read decimal digits from stdin */
    while (bytes_read < 5) {
        int input = semihost_read_byte(dev);
        c = (uint8_t)input;
        if (input < 0) {
            break;
        }
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c >= '0' && c <= '9') {
            buffer[bytes_read++] = c;
        }
    }
    buffer[bytes_read] = '\0';

    /* Parse the integer */
    if (bytes_read > 0) {
        value = (uint16_t)atoi(buffer);
    }

    return value;
}

static uint8_t semihost_io_read(device_t* dev, uint32_t addr)
{
    (void)addr;
    semihost_t* semihost = (semihost_t*)dev;

    if (!semihost->cpu) {
        return 0x00;
    }

    uint8_t operation = semihost->cpu->a;

    switch (operation) {
        case SEMIHOST_READ_CHAR: {
            uint8_t result = semihost_op_read_char(semihost);
            return result;
        }
        case SEMIHOST_READ_INT: {
            uint16_t result = semihost_op_read_int(semihost);
            semihost->cpu->h = (result >> 8);
            semihost->cpu->l = (result & 0xff);
            return 0x00;
        }
        case SEMIHOST_VERSION:
            return 0x01;
        default:
            break;
    }

    return 0x00;
}

/* Writes have always been disabled. Do not dispatch inactive operations. */
static void semihost_io_write(device_t* dev, uint32_t addr, uint8_t data)
{
    (void)dev;
    (void)addr;
    (void)data;
}

int semihost_init(semihost_t* dev, z80* cpu)
{
    dev->cpu = cpu;
    
    /* Initialize all performance counters */
    for (int i = 0; i < SEMIHOST_MAX_COUNTERS; i++) {
        semihost_counter_reset_stats(&dev->counters[i]);
        dev->counters[i].break_on_update = false;
    }
    
    device_init_io(DEVICE(dev), "semihost", semihost_io_read, semihost_io_write, 1);
    return 0;
}
