/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "hw/device.h"

typedef struct z80 z80;

#define SEMIHOST_MAX_COUNTERS 8

typedef struct {
    uint64_t start_cyc;  /* t-states value when counter was started */
    uint64_t last_split_cyc; /* t-states value at last split/start */
    bool running;             /* whether counter is currently running */
} semihost_counter_t;

/**
 * @brief Semihosting operations (written to register A)
 */
typedef enum {
    SEMIHOST_EXIT          = 0x00,  /* Exit emulator, L as error code */
    SEMIHOST_PRINT_CHAR    = 0x01,  /* Print register L as a character */
    SEMIHOST_PRINT_INT     = 0x02,  /* Print HL as a 16-bit decimal integer */
    SEMIHOST_PRINT_STRING  = 0x03,  /* Print null-terminated string at memory address HL */
    SEMIHOST_READ_CHAR     = 0x04,  /* Read a character from standard input into A */
    SEMIHOST_READ_STRING   = 0x05,  /* Read null-terminated string from stdin into memory at HL */
    SEMIHOST_PRINT_HEX     = 0x06,  /* Print HL as hexadecimal */
    SEMIHOST_READ_INT      = 0x07,  /* Read 16-bit integer from stdin into HL */
    SEMIHOST_COUNTER_START = 0x08,  /* Start t-states counter: L = counter ID (0-7) */
    SEMIHOST_COUNTER_STOP  = 0x09,  /* Stop counter and print elapsed t-states: L = counter ID */
    SEMIHOST_COUNTER_SPLIT = 0x0A, /* Split counter (print since last split, don't stop): L = counter ID */
    SEMIHOST_VERSION       = 0xFF,  /* Return semihost version in A */
} semihost_op_t;

typedef struct {
    device_t parent;
    z80* cpu;                                       /* Pointer to CPU registers for register access */
    semihost_counter_t counters[SEMIHOST_MAX_COUNTERS]; /* Array of performance counters */
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
