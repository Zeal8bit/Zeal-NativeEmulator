/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"

#define NOR_FLASH_SIZE_KB_MAX (512 * 1024)
#if CONFIG_NOR_FLASH_512KB
#define NOR_FLASH_SIZE_KB NOR_FLASH_SIZE_KB_MAX
#else
#define NOR_FLASH_SIZE_KB (NOR_FLASH_SIZE_KB_MAX / 2)
#endif

/**
 * @file Emulation for the NOR Flash (SST39)
 */

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
    /* Number of ticks remaining in case of a delay */
    int ticks_remaining;
    /* Flag set if any byte was changed (and needs write-back) */
    int dirty;
} flash_t;


int flash_init(flash_t* flash);

void flash_tick(flash_t* flash, int elapsed_tstates);

int flash_load_from_file(flash_t* flash, const char* rom_filename, const char* userprog_filename);

int flash_save_to_file(flash_t* flash, const char* name);
