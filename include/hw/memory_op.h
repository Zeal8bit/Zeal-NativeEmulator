/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>

typedef struct {
    uint8_t (*read_byte)(void*, uint16_t);
    void (*write_byte)(void*, uint16_t, uint8_t);
    uint8_t (*phys_read_byte)(void*, uint32_t);
    void (*phys_write_byte)(void*, uint32_t, uint8_t);
    void* opaque;
} memory_op_t;


static inline uint8_t memory_read_byte(const memory_op_t* ops, uint16_t addr)
{
    return ops->read_byte(ops->opaque, addr);
}


static inline void memory_read_bytes(const memory_op_t* ops, uint16_t addr, uint8_t* values, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        values[i] = ops->read_byte(ops->opaque, addr + i);
    }
}

static inline void memory_write_byte(const memory_op_t* ops, uint16_t addr, uint8_t value)
{
    ops->write_byte(ops->opaque, addr, value);
}


static inline void memory_write_bytes(const memory_op_t* ops, uint16_t addr, uint8_t* values, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        ops->write_byte(ops->opaque, addr + i, values[i]);
    }
}


static inline uint8_t memory_phys_read_byte(const memory_op_t* ops, uint32_t addr)
{
    return ops->phys_read_byte(ops->opaque, addr);
}

static inline void memory_phys_read_bytes(const memory_op_t* ops, uint32_t addr, uint8_t* values, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        values[i] = ops->phys_read_byte(ops->opaque, addr + i);
    }
}

static inline void memory_phys_write_byte(const memory_op_t* ops, uint32_t addr, uint8_t value)
{
    ops->phys_write_byte(ops->opaque, addr, value);
}

static inline void memory_phys_write_bytes(const memory_op_t* ops, uint32_t addr, uint8_t* values, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        ops->phys_write_byte(ops->opaque, addr + i, values[i]);
    }
}
