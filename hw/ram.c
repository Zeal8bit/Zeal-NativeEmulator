/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/ram.h"
#include "hw/host.h"


static uint8_t ram_read(device_t* dev, uint32_t addr) {
    ram_t* r = (ram_t*)dev;
    if(addr >= r->size) {
        hw_log_error(r->parent.host, "[RAM] Invalid read size: %08x\n", addr);
        return 0;
    }
    return r->data[addr];
}


static void ram_write(device_t* dev, uint32_t addr, uint8_t data) {
    ram_t* r = (ram_t*)dev;
    if(addr >= r->size) {
        hw_log_error(r->parent.host, "[RAM] Invalid read size: %08x\n", addr);
        return;
    }
    r->data[addr] = data;
}


static bool ram_dma_access_can_defer(device_t* dev, uint32_t addr)
{
    const ram_t* ram = (ram_t*)dev;
    return addr < ram->size;
}

int ram_init(ram_t* r, const hw_host_t* host)
{
    if(r == NULL) {
        return 1;
    }

    memset(r, 0, sizeof(*r));
    r->parent.host = host;
    r->size = RAM_SIZE_KB;

#if CONFIG_RAM_DYNAMIC_ARRAY
    r->data = malloc(r->size);
    if (r->data == NULL) {
        hw_log_error(r->parent.host, "[RAM] ERROR: could not allocate enough memory for the RAM\n");
        return 2;
    }
#endif

    device_init_mem(DEVICE(r), "ram_dev", ram_read, ram_write, r->size);
    r->parent.mem_region.dma_access_can_defer = ram_dma_access_can_defer;
    return 0;
}

void ram_deinit(ram_t* r)
{
    if (r == NULL) {
        return;
    }
#if CONFIG_RAM_DYNAMIC_ARRAY
    free(r->data);
    r->data = NULL;
#endif
}
