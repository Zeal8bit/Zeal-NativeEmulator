/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#define DEVICE(dev) &((dev)->parent)

typedef struct device_t device_t;

/**
 * @brief Structure defining a region in memory
 */
typedef struct {
    uint8_t (*read)(device_t* dev, uint32_t addr);
    uint8_t (*debug_read)(device_t* dev, uint32_t addr); /* Same as read but valid for write-only areas */
    void (*write)(device_t* dev, uint32_t addr, uint8_t data);
    int size;
    uint8_t upper_addr;
} region_t;

struct device_t {
    const char* name; // Used for debugging
    region_t io_region;
    region_t mem_region;
    void (*reset)(device_t* dev);
};


static inline void device_init_io(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                  void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name = name;
    dev->io_region = (region_t) {
        .size  = size,
        .read  = read,
        .write = write,
    };
}


static inline void device_init_mem(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                   void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name = name;
    dev->mem_region = (region_t) {
        .size  = size,
        .read  = read,
        .write = write,
    };
}

static inline void device_init_mem_debug(device_t* dev, const char* name,
                                         uint8_t (*read)(device_t*, uint32_t),
                                         void (*write)(device_t*, uint32_t, uint8_t),
                                         uint8_t (*dbg_read)(device_t*, uint32_t),
                                         int size)
{
    dev->name = name;
    dev->mem_region = (region_t) {
        .size  = size,
        .read  = read,
        .write = write,
        .debug_read = dbg_read,
    };
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
