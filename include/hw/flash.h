#pragma once

#include <stdint.h>
#include "hw/device.h"

#if CONFIG_NOR_FLASH_512KB
    #define NOR_FLASH_SIZE_KB 512
#else
    #define NOR_FLASH_SIZE_KB 256
#endif

/**
 * @file Emulation for the NOR Flash (SST39)
 */

typedef struct {
    device_t    parent;
    size_t      size;   // in bytes
#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    uint8_t*    data;
#else
    uint8_t     data[NOR_FLASH_SIZE_KB];
#endif
} flash_t;


int flash_init(flash_t*);

int flash_load_from_file(flash_t*, const char* name);

int flash_save_to_file(flash_t*, const char* name);