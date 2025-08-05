/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t *array;
    int rd;
    int wr;
    bool empty;
    size_t size;
} fifo_t;


bool fifo_init(fifo_t *fifo, size_t size);

void fifo_deinit(fifo_t *fifo);

void fifo_reset(fifo_t *fifo);

bool fifo_push(fifo_t *fifo, uint8_t value);

bool fifo_pop(fifo_t *fifo, uint8_t *value);

size_t fifo_size(fifo_t *fifo);
