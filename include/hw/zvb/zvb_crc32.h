/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/pio.h"

typedef struct {
    uint32_t sum;
} zvb_crc32_t;

int zvb_crc32_init(zvb_crc32_t* crc32);

void zvb_crc32_reset(zvb_crc32_t* crc32);

void zvb_crc32_write(zvb_crc32_t* crc32, uint16_t subaddr, uint8_t data);

uint8_t zvb_crc32_read(zvb_crc32_t* crc32, uint16_t subaddr);
