/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hw/semihost.h"
#include "hw/z80.h"
#include "hw/device.h"

#define SEMIHOST_MAX_STRING_LEN 256

/**
 * @brief Helper: combine two registers into a 16-bit value
 */
static inline uint16_t concat_registers(uint8_t h, uint8_t l)
{
    return (h << 8) | l;
}

/**
 * @brief Helper: extract high byte from 16-bit value
 */
static inline uint8_t get_high_byte(uint16_t value)
{
    return (value >> 8) & 0xFF;
}

/**
 * @brief Helper: extract low byte from 16-bit value
 */
static inline uint8_t get_low_byte(uint16_t value)
{
    return value & 0xFF;
}

/**
 * @brief Read a byte from memory via the CPU's read_byte callback
 */
static uint8_t semihost_read_mem(semihost_t* dev, uint16_t addr)
{
    if (dev->cpu && dev->cpu->read_byte) {
        return dev->cpu->read_byte(dev->cpu->userdata, addr);
    }
    return 0;
}

/**
 * @brief Write a byte to memory via the CPU's write_byte callback
 */
static void semihost_write_mem(semihost_t* dev, uint16_t addr, uint8_t value)
{
    if (dev->cpu && dev->cpu->write_byte) {
        dev->cpu->write_byte(dev->cpu->userdata, addr, value);
    }
}

/**
 * @brief Operation: EXIT - terminate the emulator
 */
static void semihost_op_exit(uint16_t exit_code)
{
    fprintf(stdout, "[SEMIHOST] EXIT(%u)\n", exit_code);
    fflush(stdout);
    exit(exit_code & 0xFF);
}

/**
 * @brief Operation: PRINT_CHAR - output a character
 */
static void semihost_op_print_char(uint8_t c)
{
    fputc(c, stdout);
    fflush(stdout);
}

/**
 * @brief Operation: PRINT_INT - output a 16-bit integer as decimal
 */
static void semihost_op_print_int(uint16_t value)
{
    fprintf(stdout, "%u", value);
    fflush(stdout);
}

/**
 * @brief Operation: PRINT_STRING - output null-terminated string from memory
 */
static void semihost_op_print_string(semihost_t* dev, uint16_t addr)
{
    uint8_t c;
    int bytes_read = 0;

    while (bytes_read < SEMIHOST_MAX_STRING_LEN) {
        c = semihost_read_mem(dev, addr++);
        if (c == 0) break;
        fputc(c, stdout);
        bytes_read++;
    }
    fflush(stdout);
}

/**
 * @brief Operation: READ_CHAR - read one character from stdin
 */
static uint8_t semihost_op_read_char(void)
{
    uint8_t c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return 0xFF;
}

/**
 * @brief Operation: READ_STRING - read null-terminated string from stdin into memory
 */
static void semihost_op_read_string(semihost_t* dev, uint16_t addr)
{
    uint8_t c;
    int bytes_read = 0;

    while (bytes_read < SEMIHOST_MAX_STRING_LEN - 1) {
        if (read(STDIN_FILENO, &c, 1) != 1) {
            break;
        }
        if (c == '\n') {
            break;
        }
        semihost_write_mem(dev, addr++, c);
        bytes_read++;
    }
    semihost_write_mem(dev, addr, 0);
}

/**
 * @brief Operation: PRINT_HEX - output a 16-bit value in hexadecimal
 */
static void semihost_op_print_hex(uint16_t value)
{
    fprintf(stdout, "0x%04X", value);
    fflush(stdout);
}

/**
 * @brief Operation: READ_INT - read a 16-bit decimal integer from stdin
 */
static uint16_t semihost_op_read_int(void)
{
    char buffer[6]; /* Max 5 digits for 16-bit unsigned (65535) */
    int bytes_read = 0;
    uint8_t c;
    uint16_t value = 0;

    /* Read decimal digits from stdin */
    while (bytes_read < 5) {
        if (read(STDIN_FILENO, &c, 1) != 1) {
            break;
        }
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c >= '0' && c <= '9') {
            buffer[bytes_read++] = c;
        }
    }
    buffer[bytes_read] = '\0';

    /* Parse the integer */
    if (bytes_read > 0) {
        value = (uint16_t)atoi(buffer);
    }

    return value;
}

/**
 * @brief I/O port read: execute read operations and return results
 */
static uint8_t semihost_io_read(device_t* dev, uint32_t addr)
{
    (void)addr;
    semihost_t* semihost = (semihost_t*)dev;

    if (!semihost->cpu) {
        return 0x00;
    }

    uint8_t operation = semihost->cpu->a;

    switch (operation) {
        case SEMIHOST_READ_CHAR: {
            uint8_t result = semihost_op_read_char();
            semihost->cpu->l = result;
            return 0x00;
        }
        case SEMIHOST_READ_INT: {
            uint16_t result = semihost_op_read_int();
            semihost->cpu->h = get_high_byte(result);
            semihost->cpu->l = get_low_byte(result);
            return 0x00;
        }
        case SEMIHOST_VERSION:
            return 0x01;
        default:
            break;
    }

    return 0x00;
}

/**
 * @brief I/O port write: execute write operations
 */
static void semihost_io_write(device_t* dev, uint32_t addr, uint8_t data)
{
    (void)addr;
    (void)data;
    semihost_t* semihost = (semihost_t*)dev;

    if (!semihost->cpu) {
        return;
    }

    uint8_t operation = semihost->cpu->a;
    uint8_t reg_l = semihost->cpu->l;
    uint8_t reg_h = semihost->cpu->h;
    uint16_t reg_hl = concat_registers(reg_h, reg_l);

    switch (operation) {
        case SEMIHOST_EXIT:
            semihost_op_exit(reg_hl);
            break;
        case SEMIHOST_PRINT_CHAR:
            semihost_op_print_char(reg_l);
            break;
        case SEMIHOST_PRINT_INT:
            semihost_op_print_int(reg_hl);
            break;
        case SEMIHOST_PRINT_STRING:
            semihost_op_print_string(semihost, reg_hl);
            break;
        case SEMIHOST_READ_STRING:
            semihost_op_read_string(semihost, reg_hl);
            break;
        case SEMIHOST_PRINT_HEX:
            semihost_op_print_hex(reg_hl);
            break;
        case SEMIHOST_FLUSH:
            fflush(stdout);
            break;
        default:
            break;
    }
}

int semihost_init(semihost_t* dev, z80* cpu)
{
    dev->cpu = cpu;
    device_init_io(DEVICE(dev), "semihost", semihost_io_read, semihost_io_write, 1);
    return 0;
}
