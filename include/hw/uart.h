/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stddef.h>

#include "hw/pio.h"
#include "hw/device.h"

#define IO_UART_RX_PIN 3
#define IO_UART_TX_PIN 4

#define BAUDRATE_US 17.361

typedef struct {
        // device_t
        device_t parent;
        size_t size; // in bytes
} uart_t;

int uart_init(uart_t* uart, pio_t* pio);
