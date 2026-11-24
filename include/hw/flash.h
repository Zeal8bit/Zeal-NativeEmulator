/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "utils/vtimer.h"

#define NOR_FLASH_SIZE_KB_MAX (512 * 1024)
#if CONFIG_NOR_FLASH_512KB
#define NOR_FLASH_SIZE_KB NOR_FLASH_SIZE_KB_MAX
#else
#define NOR_FLASH_SIZE_KB (NOR_FLASH_SIZE_KB_MAX / 2)
#endif

/**
 * @file Emulation for the NOR Flash (SST39)
 */


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    device_t parent;
    size_t size; // in bytes
#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    uint8_t* data;
#else
    uint8_t data[NOR_FLASH_SIZE_KB];
#endif
    int state;
    /* Byte being written to flash */
    uint8_t writing_byte;
    /* Virtual timer for write/erase delays */
    vtimer_node_t timer;
    /* Flag set if any byte was changed (and needs write-back) */
    int dirty;
} flash_t;


int flash_init(flash_t* flash, vtimer_t* clock, const hw_host_t* host);

/* Release the flash storage allocated by flash_init (dynamic-array builds). */
void flash_deinit(flash_t* flash);

/* Mark an externally loaded image as clean without changing pending timers. */
void flash_loaded(flash_t* flash);

#ifdef __cplusplus
} /* extern "C" */
#endif
