/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stddef.h>
#include "hw/host.h"
#include <stdint.h>
#include <stdbool.h>
#include "utils/vtimer.h"

#define SPI_RAM_LEN     8
#define SPI_VERSION     0

#define TF_DATA_TOKEN   0xFE

/**
 * @brief I/O registers address, relative to the controller
 */
#define SPI_REG_VERSION     0
#define SPI_REG_CTRL        1
    #define SPI_REG_CTRL_START      (1 << 7)    // Start SPI transaction
    #define SPI_REG_CTRL_RESET      (1 << 6)    // Reset the SPI controller
    #define SPI_REG_CTRL_CS_START   (1 << 5)    // Assert chip select (low)
    #define SPI_REG_CTRL_CS_END     (1 << 4)    // De-assert chip select signal (high)
    #define SPI_REG_CTRL_CS_SEL     (1 << 3)    // Select among two chip selects (0 for TF card, 1 is unused)
    #define SPI_REG_CTRL_RSV2       (1 << 2)
    #define SPI_REG_CTRL_RSV1       (1 << 1)
    #define SPI_REG_CTRL_STATE      (1 << 0)    // SPI controller in BUSY state if 1, IDLE if 0
#define SPI_REG_CLK_DIV     2
#define SPI_REG_RAM_LEN     3
#define SPI_REG_OPERAND     4
#define SPI_REG_RAM_FIFO    7
#define SPI_REG_RAM_FROM    8
#define SPI_REG_RAM_TO      15


typedef union {
    struct {
        uint32_t busy : 1;
        uint32_t rsv1 : 1;
        uint32_t rsv2 : 1;
        uint32_t csel : 1;   // 0 for TF card, 1 is unused
        uint32_t cend : 1;   // De-assert chip select signal (high)
        uint32_t cstart : 1; // Assert the chip select (low)
        uint32_t reset : 1;  // Reset the controller
        uint32_t start : 1;  // Start SPI transaction
    };
    uint32_t raw;
} zvb_spi_ctrl_t;


typedef struct {
    uint8_t data[SPI_RAM_LEN];
    uint8_t idx;
} zvb_spi_ram_t;


/**
 * @brief States for the FSM representing the TF Card
 */
typedef enum {
    TF_WAIT_IDLE,
    TF_IDLE,
    TF_CMD55_RECEIVED,
    TF_READ_BLOCK,
    TF_WRITE_BLOCK_WAIT_TOK,
    TF_WRITE_BLOCK,
    TF_WRITE_BLOCK_SEND_RESP,
} zvb_tf_state_t;


static inline int zvb_tf_is_write(zvb_tf_state_t st) {
    return st == TF_WRITE_BLOCK_WAIT_TOK ||
           st == TF_WRITE_BLOCK ||
           st == TF_WRITE_BLOCK_SEND_RESP;
}


/* Borrowed media callbacks. A successful seek selects one complete sector;
 * completed writes are flushed before the guest receives its response.
 * Supply all callbacks for a connected card, or an all-zero backend to detach.
 * Reads/writes span 512 bytes and return a byte count; flush returns zero on
 * success. The backend and its userdata must remain valid until detached. */
typedef struct {
    void* userdata;
    size_t size;
    bool (*seek)(void*, uint32_t sector);
    size_t (*read)(void*, uint8_t* sector);
    size_t (*write)(void*, const uint8_t* sector);
    int (*flush)(void*);
} zvb_tf_backend_t;

/**
 * @brief Structure representing a TF card
 */
typedef struct {
    zvb_tf_state_t  state;
    uint32_t debug_command;
    uint32_t debug_parameter;
    int debug_count;
    zvb_tf_backend_t backend;
    const hw_host_t* host;
    /* Managed by the SPI controller */
    uint8_t         reply[1024];
    int             reply_idx;
    int             reply_len;
    uint8_t         command[6];
    uint32_t        command_index;
    uint32_t        write_index;
} zvb_tf_t;


/**
 * @brief Type for the SPI controller
 */
typedef struct {
    uint8_t         clk_div;
    uint8_t         ram_len;
    uint8_t         operand;
    zvb_spi_ram_t   ram_rd;
    zvb_spi_ram_t   ram_wr;
    /* When 1, the TF chip select line is asserted */
    uint8_t         tf_cs;
    zvb_tf_t        tf;
    bool            busy;
    uint8_t         transfer_index;
    uint8_t         transfer_len;
    uint8_t         outgoing;
    uint8_t         sampled_bits;
    uint32_t        half_period_ns;
    uint64_t        byte_started_ns;
    vtimer_node_t   event;
} zvb_spi_t;


/**
 * @brief Initialize the SPI, must be called before using it.
 */
void zvb_spi_init(zvb_spi_t* spi, vtimer_t* clock);


/**
 * @brief Simulate a hardware reset on the SPI controller.
 */
void zvb_spi_reset(zvb_spi_t* spi);


/**
 * @brief Function to call when a write occurs on the SPI I/O controller.
 *
 * @param addr Address relative to the SPI address space.
 * @param data Byte to write
 */
void zvb_spi_write(zvb_spi_t* spi, uint32_t addr, uint8_t value);

/**
 * @brief Function to call when a read occurs on the SPI I/O controller.
 *
 * @param addr Address relative to the SPI address space.
 */
uint8_t zvb_spi_read(zvb_spi_t* spi, uint32_t addr);


void zvb_spi_bind_tf(zvb_spi_t* spi, const zvb_tf_backend_t* backend);
