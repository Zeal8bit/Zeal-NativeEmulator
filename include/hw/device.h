/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hw/host.h"

#define DEVICE(dev) &((dev)->parent)


#ifdef __cplusplus
extern "C" {
#endif

typedef struct device_t device_t;

/**
 * @brief Structure defining a region in memory
 */
typedef struct {
    uint8_t (*read)(device_t* dev, uint32_t addr);
    uint8_t (*debug_read)(device_t* dev, uint32_t addr); /* Same as read but valid for write-only areas */
    void (*write)(device_t* dev, uint32_t addr, uint8_t data);
    /* All mapped devices support DMA through ordinary read/write callbacks.
     * This only permits deferring accesses within an event-free time window;
     * absent/false keeps side-effecting accesses individually ordered. */
    bool (*dma_access_can_defer)(device_t* dev, uint32_t addr);
    int size;
    uint8_t upper_addr;
} region_t;

struct device_t {
    const hw_host_t* host;
    const char* name; // Used for debugging
    region_t io_region;
    region_t mem_region;
    void (*reset)(device_t* dev);
};


static inline void device_init_io(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                  void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name = name;
    memset(&dev->io_region, 0, sizeof(dev->io_region));
    dev->io_region.size  = size;
    dev->io_region.read  = read;
    dev->io_region.write = write;
}


static inline void device_init_mem(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                   void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name = name;
    memset(&dev->mem_region, 0, sizeof(dev->mem_region));
    dev->mem_region.size  = size;
    dev->mem_region.read  = read;
    dev->mem_region.write = write;
}

static inline void device_init_mem_debug(device_t* dev, const char* name,
                                         uint8_t (*read)(device_t*, uint32_t),
                                         void (*write)(device_t*, uint32_t, uint8_t),
                                         uint8_t (*dbg_read)(device_t*, uint32_t),
                                         int size)
{
    dev->name = name;
    memset(&dev->mem_region, 0, sizeof(dev->mem_region));
    dev->mem_region.size       = size;
    dev->mem_region.read       = read;
    dev->mem_region.write      = write;
    dev->mem_region.debug_read = dbg_read;
}

static inline void device_register_reset(device_t* dev, void (*reset)(device_t* dev))
{
    dev->reset = reset;
}

static inline void device_reset(device_t* dev)
{
    if (dev && dev->reset) {
        dev->reset(dev);
    }
}

#ifdef __cplusplus
} /* extern "C" */
#endif
