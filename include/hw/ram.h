/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"

#define RAM_SIZE_KB (512 * 1024)

/**
 * @file Emulation for the RAM (Alliance AS6C4008)
 */


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        device_t parent;
        size_t size; // in bytes
#if CONFIG_RAM_DYNAMIC_ARRAY
        uint8_t* data;
#else
        uint8_t data[RAM_SIZE_KB];
#endif
} ram_t;

int ram_init(ram_t*, const hw_host_t* host);

/* Release the RAM storage allocated by ram_init (dynamic-array builds). */
void ram_deinit(ram_t* ram);

#ifdef __cplusplus
} /* extern "C" */
#endif
