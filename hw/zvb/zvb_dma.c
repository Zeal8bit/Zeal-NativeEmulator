/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include "hw/mmu.h"
#include "hw/zvb/zvb_dma.h"

#define DMA_DESC_SRC_LOW                 (0)
#define DMA_DESC_SRC_MID                 (1)
#define DMA_DESC_SRC_HIGH                (2)
#define DMA_DESC_DST_LOW                 (3)
#define DMA_DESC_DST_MID                 (4)
#define DMA_DESC_DST_HIGH                (5)
#define DMA_DESC_LENGTH_LOW              (6)
#define DMA_DESC_LENGTH_HIGH             (7)
#define DMA_DESC_FLAGS                   (8)
#define DMA_DESC_PADDING                 (3)
#define DMA_ADDRESS_HIGH_MASK            (0x3f)
#define DMA_LAST_FLAG                    (1)
#define DMA_OPERATION_MASK               (3)
#define DMA_READ_OPERATION_SHIFT         (1)
#define DMA_WRITE_OPERATION_SHIFT        (3)
#define DMA_RESET_CLOCK_DIVIDER          (0x56)

#define DMA_CLOCK_NS 20UL
#define DMA_ADDRESS_MASK (MEM_SPACE_SIZE - 1u)

static void dma_schedule(zvb_dma_t* dma, uint32_t clocks)
{
    vtimer_schedule_ns(&dma->timer, clocks * DMA_CLOCK_NS);
}


static inline uint32_t dma_next_address(uint32_t address, uint32_t operation)
{
    if (operation == DMA_OP_INC) {
        address++;
    } else if (operation == DMA_OP_DEC) {
        address--;
    }
    return address & DMA_ADDRESS_MASK;
}


static void dma_advance(void* userdata)
{
    zvb_dma_t* dma = userdata;
    const uint32_t read_clocks = 1u + dma->clk.rd_cycle;
    const uint32_t write_clocks = 1u + dma->clk.wr_cycle;

    switch (dma->state) {
        case DMA_REQUEST:
            /* Initial ACK is accepted at the triggering instruction boundary.
             * Chained requests take one additional FPGA clock. */
            dma->descriptor_index = 0;
            dma->state = DMA_DESCRIPTOR;
            dma_schedule(dma, read_clocks);
            break;

        case DMA_DESCRIPTOR: {
            const uint8_t value = mmu_phys_read_byte(dma->mmu, dma->desc_addr);
            dma->desc_addr = (dma->desc_addr + 1) & DMA_ADDRESS_MASK;
            switch (dma->descriptor_index++) {
                case DMA_DESC_SRC_LOW:
                    dma->rd_addr = value;
                    break;
                case DMA_DESC_SRC_MID:
                    dma->rd_addr |= (uint32_t)value << 8;
                    break;
                case DMA_DESC_SRC_HIGH:
                    dma->rd_addr |= (uint32_t)(value & DMA_ADDRESS_HIGH_MASK) << 16;
                    break;
                case DMA_DESC_DST_LOW:
                    dma->wr_addr = value;
                    break;
                case DMA_DESC_DST_MID:
                    dma->wr_addr |= (uint32_t)value << 8;
                    break;
                case DMA_DESC_DST_HIGH:
                    dma->wr_addr |= (uint32_t)(value & DMA_ADDRESS_HIGH_MASK) << 16;
                    break;
                case DMA_DESC_LENGTH_LOW:
                    dma->remaining = value;
                    break;
                case DMA_DESC_LENGTH_HIGH:
                    dma->remaining |= (uint16_t)value << 8;
                    break;
                case DMA_DESC_FLAGS: {
                    /* The RTL skips padding and, for zero length, tests the
                     * previous flags register before its nonblocking update. */
                    const uint8_t previous_flags = dma->flags;
                    dma->flags = value;
                    dma->desc_addr = (dma->desc_addr + DMA_DESC_PADDING) & DMA_ADDRESS_MASK;
                    if (dma->remaining == 0) {
                        dma->state = (previous_flags & DMA_LAST_FLAG) ? DMA_RELEASE : DMA_REQUEST;
                        dma_schedule(dma, 1);
                    } else {
                        dma->state = DMA_READ;
                        dma_schedule(dma, read_clocks);
                    }
                    return;
                }
            }
            dma_schedule(dma, read_clocks);
            break;
        }

        case DMA_READ:
            dma->data = mmu_phys_read_byte(dma->mmu, dma->rd_addr);
            dma->state = DMA_WRITE;
            dma_schedule(dma, write_clocks);
            break;

        case DMA_WRITE:
            mmu_phys_write_byte(dma->mmu, dma->wr_addr, dma->data);
            dma->rd_addr = dma_next_address(dma->rd_addr, (dma->flags >> DMA_READ_OPERATION_SHIFT) & DMA_OPERATION_MASK);
            dma->wr_addr = dma_next_address(dma->wr_addr, (dma->flags >> DMA_WRITE_OPERATION_SHIFT) & DMA_OPERATION_MASK);
            if (--dma->remaining == 0) {
                dma->state = (dma->flags & DMA_LAST_FLAG) ? DMA_RELEASE : DMA_REQUEST;
                dma_schedule(dma, 1);
            } else {
                dma->state = DMA_READ;
                dma_schedule(dma, read_clocks);
            }
            break;

        case DMA_RELEASE:
            dma->state = DMA_IDLE;
            dma->mmu->bus_requested = false;
            break;

        case DMA_IDLE:
            break;
    }
}


void zvb_dma_init(zvb_dma_t* dma, mmu_t* mmu, bool bus_connected)
{
    dma->desc_addr = 0;
    dma->flags = 0;
    dma->mmu = mmu;
    dma->bus_connected = bus_connected;
    vtimer_init_node(&dma->timer, dma_advance, dma);
    zvb_dma_reset(dma);
}


void zvb_dma_reset(zvb_dma_t* dma)
{
    vtimer_cancel(&dma->timer);
    dma->state = DMA_IDLE;
    dma->mmu->bus_requested = false;
    dma->remaining = 0;
    dma->wr_addr = 0;
    dma->rd_addr = 0;
    /* CLK_DIV reset value in ZVB 1.0.0; descriptor address is unchanged. */
    dma->clk.raw = DMA_RESET_CLOCK_DIVIDER;
}


uint8_t zvb_dma_read(zvb_dma_t* dma, uint32_t port)
{
    switch (port) {
        case DMA_REG_DESC_ADDR0: return dma->desc_addr0;
        case DMA_REG_DESC_ADDR1: return dma->desc_addr1;
        case DMA_REG_DESC_ADDR2: return dma->desc_addr2;
        case DMA_REG_CLK_DIV:    return dma->clk.raw;
        default: return 0;
    }
}


void zvb_dma_write(zvb_dma_t* dma, uint32_t port, uint8_t value)
{
    switch (port) {
        case DMA_REG_CTRL:
            if ((value & DMA_CTRL_START) != 0) {
                if (dma->state == DMA_IDLE) {
                    dma->state = DMA_REQUEST;
                    /* A disconnected BUSREQ never reaches the CPU and no ACK
                     * returns. Keep the request pending without advancing it. */
                    if (dma->bus_connected) {
                        dma->mmu->bus_requested = true;
                        /* Defer ACK until the CPU finishes the OUT instruction. */
                        dma_schedule(dma, 0);
                    }
                }
            }
            break;
        case DMA_REG_DESC_ADDR0:
            dma->desc_addr0 = value;
            break;
        case DMA_REG_DESC_ADDR1:
            dma->desc_addr1 = value;
            break;
        case DMA_REG_DESC_ADDR2:
            dma->desc_addr2 = value & DMA_ADDRESS_HIGH_MASK;
            break;
        case DMA_REG_CLK_DIV:
            dma->clk.raw = value;
            break;
        default:
            break;
    }
}
