#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/log.h"
#include "hw/zvb/zvb_dma.h"

#define DEBUG_DMA   0

static void dma_start_transfer(zvb_dma_t* dma)
{
    /* Grab the first descriptor from memory */
    zvb_dma_descriptor_t desc = { 0 };

    do {
        memory_phys_read_bytes(dma->ops, dma->desc_addr, (void*) &desc, sizeof(zvb_dma_descriptor_t));
        const int rd_ops = desc.flags.rd_op;
        const int wr_ops = desc.flags.wr_op;

#if DEBUG_DMA
        log_printf("Descriptor @ %08x:\n", dma->desc_addr);
        log_printf("  Read Address: 0x%08X\n", desc.rd_addr);
        log_printf("  Write Address: 0x%08X\n", desc.wr_addr);
        log_printf("  Length: %d\n", desc.length);
        log_printf("  Flags:\n");
        log_printf("    Read Operation: %d\n", desc.flags.rd_op);
        log_printf("    Write Operation: %d\n", desc.flags.wr_op);
        log_printf("    Last: %d\n", desc.flags.last);
#endif

        /* Descriptor is ready, perform the copy */
        for (int i = 0; i < desc.length; i++) {
            const uint8_t data = memory_phys_read_byte(dma->ops, desc.rd_addr);
            memory_phys_write_byte(dma->ops, desc.wr_addr, data);

#if DEBUG_DMA
            log_printf("Transfer: src=0x%08X, dst=0x%08X, byte=0x%02X\n", desc.rd_addr, desc.wr_addr, data);
#endif

            /* Check if we have to modify the addresses */
            if (rd_ops == DMA_OP_INC)      desc.rd_addr++;
            else if (rd_ops == DMA_OP_DEC) desc.rd_addr--;

            if (wr_ops == DMA_OP_INC)      desc.wr_addr++;
            else if (wr_ops == DMA_OP_DEC) desc.wr_addr--;
        }
        /* Make the descriptor pointer go to the next descriptor */
        dma->desc_addr += sizeof(zvb_dma_descriptor_t);
    } while (!desc.flags.last);

    /* TODO: add number of elapsed T-states to the CPU? */
}


void zvb_dma_init(zvb_dma_t* dma, const memory_op_t* ops)
{
    dma->clk.rd_cycle = 1;
    dma->clk.wr_cycle = 1;
    dma->desc_addr = 0;
    dma->ops = ops;
}

void zvb_dma_reset(zvb_dma_t* dma)
{
    /* Different than boot values */
    dma->clk.rd_cycle = 6;
    dma->clk.wr_cycle = 5;
    /* Descriptor address unchanged on reset */
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
                dma_start_transfer(dma);
            }
            break;
        case DMA_REG_DESC_ADDR0:
            dma->desc_addr0 = value;
            break;
        case DMA_REG_DESC_ADDR1:
            dma->desc_addr1 = value;
            break;
        case DMA_REG_DESC_ADDR2:
            dma->desc_addr2 = value;
            break;
        case DMA_REG_CLK_DIV:
            dma->clk.raw = value;
            break;
        default:
            break;
    }
}
