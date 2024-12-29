#pragma once

#include <stdint.h>
#include "hw/device.h"

#define RAM_SIZE_KB (512 * 1024)

/**
 * @file Emulation for the RAM (Alliance AS6C4008)
 */

typedef struct {
        device_t parent;
        size_t size; // in bytes
#if CONFIG_RAM_DYNAMIC_ARRAY
        uint8_t* data;
#else
        uint8_t data[RAM_SIZE_KB];
#endif
} ram_t;

int ram_init(ram_t *);
