/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/memory_op.h"

/**
 * @brief I/O registers address, relative to the controller
 */
#define DMA_REG_CTRL       0x0
#define DMA_REG_DESC_ADDR0 0x1
#define DMA_REG_DESC_ADDR1 0x2
#define DMA_REG_DESC_ADDR2 0x3
#define DMA_REG_CLK_DIV    0x9

#define DMA_CTRL_START     0x80

#define DMA_OP_INC  0
#define DMA_OP_DEC  1

/**
 * @brief DMA clock register type
 */
typedef union {
    struct {
        uint8_t rd_cycle : 4;
        uint8_t wr_cycle : 4;
    };
    uint8_t raw;
} zvb_dma_clk_t;


typedef union {
    struct {
        uint8_t last  : 1;
        uint8_t rd_op : 2;
        uint8_t wr_op : 2;
        uint8_t rsd   : 3;
    };
    uint8_t raw;
} dma_flags_t;

_Static_assert(sizeof(dma_flags_t) == 1, "DMA flags must be 1 byte big");

/**
 * @brief DMA descriptors array pointed by the `desc_addr` field in the controller
 */
typedef struct {
    uint32_t rd_addr    : 24; // Source address
    uint32_t wr_addr    : 24; // Destination address
    uint16_t length     : 16; // Transfer length in bytes
    dma_flags_t flags;        // Descriptor flags
    uint32_t reserved   : 24; // Padding/future use
} __attribute__((packed)) zvb_dma_descriptor_t;

_Static_assert(sizeof(zvb_dma_descriptor_t) == 12, "DMA descriptors must be 12 bytes big");


typedef struct {
    /* Address of the current DMA descriptor */
    union {
        struct {
            uint8_t desc_addr0;
            uint8_t desc_addr1;
            uint8_t desc_addr2;
            uint8_t desc_rvd;
        };
        uint32_t desc_addr;
    };
    /* Clock divider for all the transfers */
    zvb_dma_clk_t      clk;
    /* Machine operations for mmeory read and write */
    const memory_op_t* ops;
} zvb_dma_t;


/**
 * @brief Initialize the DMA controller
 */
void zvb_dma_init(zvb_dma_t* dma, const memory_op_t* ops);


/**
 * @brief Simulate a hardware reset on the DMA controller.
 */
void zvb_dma_reset(zvb_dma_t* dma);


/**
 * @brief Function to call when a read occurs on the dma I/O controller.
 *
 * @param port DMA register to read
 */
uint8_t zvb_dma_read(zvb_dma_t* dma, uint32_t port);


/**
 * @brief Function to call when a write occurs on the DMA I/O controller.
 *
 * @param port DMA register to write
 * @param value Value of the register
 */
void zvb_dma_write(zvb_dma_t* dma, uint32_t port, uint8_t value);
