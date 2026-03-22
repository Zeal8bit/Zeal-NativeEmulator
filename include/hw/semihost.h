/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "hw/device.h"

typedef struct z80 z80;

/**
 * @brief Semihosting operations (written to register A)
 */
typedef enum {
    SEMIHOST_EXIT         = 0x00,   /* Exit emulator */
    SEMIHOST_PRINT_CHAR   = 0x01,   /* Print register L as a character */
    SEMIHOST_PRINT_INT    = 0x02,   /* Print HL as a 16-bit decimal integer */
    SEMIHOST_PRINT_STRING = 0x03,   /* Print null-terminated string at memory address HL */
    SEMIHOST_READ_CHAR    = 0x04,   /* Read a character from stdin into L */
    SEMIHOST_READ_STRING  = 0x05,   /* Read null-terminated string from stdin into memory at HL */
    SEMIHOST_PRINT_HEX    = 0x06,   /* Print HL as hexadecimal */
    SEMIHOST_READ_INT     = 0x07,   /* Read 16-bit integer from stdin into HL */
    SEMIHOST_FLUSH        = 0x08,   /* Flush output */
    SEMIHOST_VERSION      = 0xFF,   /* Return semihost version in A */
} semihost_op_t;

typedef struct {
    device_t parent;
    z80* cpu;                        /* Pointer to CPU registers for register access */
} semihost_t;

/**
 * @brief Initialize the semihosting device
 * 
 * The semihosting device provides advanced I/O operations that will be handled by the emulator
 *
 * @param dev Semihosting device structure
 * @param cpu Pointer to Z80 CPU for register access
 * @return 0 on success, negative on failure
 */
int semihost_init(semihost_t* dev, z80* cpu);
