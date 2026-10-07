/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include "hw/mmu.h"
#include "hw/zvb/zvb_dma.h"

#define DMA_CLOCK_NS 20UL
#define DMA_ADDRESS_MASK (MEM_SPACE_SIZE - 1u)

static void dma_schedule(zvb_dma_t* dma, unsigned clocks)
{
    vtimer_schedule_ns(&dma->timer, clocks * DMA_CLOCK_NS);
}


static uint32_t dma_next_address(uint32_t address, unsigned operation)
{
    if (operation == DMA_OP_INC) address++;
    else if (operation == DMA_OP_DEC) address--;
    return address & DMA_ADDRESS_MASK;
}


static void dma_advance(void* userdata)
{
    zvb_dma_t* dma = userdata;
    const unsigned read_clocks = 1u + dma->clk.rd_cycle;
    const unsigned write_clocks = 1u + dma->clk.wr_cycle;

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
                case 0: dma->rd_addr = value; break;
                case 1: dma->rd_addr |= (uint32_t)value << 8; break;
                case 2: dma->rd_addr |= (uint32_t)(value & 0x3f) << 16; break;
                case 3: dma->wr_addr = value; break;
                case 4: dma->wr_addr |= (uint32_t)value << 8; break;
                case 5: dma->wr_addr |= (uint32_t)(value & 0x3f) << 16; break;
                case 6: dma->remaining = value; break;
                case 7: dma->remaining |= (uint16_t)value << 8; break;
                case 8: {
                    /* The RTL skips padding and, for zero length, tests the
                     * previous flags register before its nonblocking update. */
                    const uint8_t previous_flags = dma->flags;
                    dma->flags = value;
                    dma->desc_addr = (dma->desc_addr + 3) & DMA_ADDRESS_MASK;
                    if (dma->remaining == 0) {
                        dma->state = (previous_flags & 1) ? DMA_RELEASE : DMA_REQUEST;
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
            dma->rd_addr = dma_next_address(dma->rd_addr, (dma->flags >> 1) & 3);
            dma->wr_addr = dma_next_address(dma->wr_addr, (dma->flags >> 3) & 3);
            if (--dma->remaining == 0) {
                dma->state = (dma->flags & 1) ? DMA_RELEASE : DMA_REQUEST;
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
    dma->rd_addr = dma->wr_addr = dma->remaining = 0;
    /* CLK_DIV reset value in ZVB 1.0.0; descriptor address is unchanged. */
    dma->clk.raw = 0x56;
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
            dma->desc_addr2 = value & 0x3f;
            break;
        case DMA_REG_CLK_DIV:
            dma->clk.raw = value;
            break;
        default:
            break;
    }
}
