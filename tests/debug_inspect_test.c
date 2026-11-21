/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Debugger inspection must not change the board: a full memory snapshot and
 * the trace's opcode lookahead read the ZVB registers without latching the
 * raster position or consuming the SPI FIFO, while guest reads still do. */

#include <stdio.h>
#include "hw/zeal.h"
#include "debugger/zeal_trace.h"

static zeal_t machine;
static uint8_t snapshot[0x10000];
static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void map_zvb(void)
{
    mmu_io_write_byte(&machine.cpu.mmu, 0xf0, ZVB_PHYS_ADDR_BASE / MMU_PAGE_SIZE);
    machine.zvb.current_scanline = 260;
    machine.zvb.ctrl.vpos_latch = 0;
    machine.zvb.spi.ram_rd.idx = 0;
    machine.zvb.peri_timer.read_latch = 0x5a;
}

int main(void)
{
    const zeal_options_t options = { .debugger = true };
    if (zeal_init(&machine, &options)) return 1;

    map_zvb();
    debugger_read_memory(&machine.dbg, 0, sizeof(snapshot), snapshot);
    CHECK(machine.zvb.ctrl.vpos_latch == 0, "snapshot latched vpos (%u)", machine.zvb.ctrl.vpos_latch);
    CHECK(machine.zvb.spi.ram_rd.idx == 0, "snapshot consumed SPI FIFO (idx %u)", machine.zvb.spi.ram_rd.idx);
    CHECK(machine.zvb.peri_timer.read_latch == 0x5a, "snapshot changed the timer latch");

#if CONFIG_TRACE_DEPTH > 0
    /* Lookahead over the configuration registers */
    for (uint16_t pc = 0x0000; pc < 0x0100; pc++) {
        machine.cpu.pc = pc;
        zeal_trace_before(&machine);
    }
    CHECK(machine.zvb.ctrl.vpos_latch == 0, "trace lookahead latched vpos");
    CHECK(machine.zvb.spi.ram_rd.idx == 0, "trace lookahead consumed SPI FIFO");
#endif

    /* The guest path keeps its side effects */
    for (uint32_t a = 0; a < 0x4000; a++) (void) mmu_virt_read_byte(&machine.cpu.mmu, (uint16_t) a);
    CHECK(machine.zvb.ctrl.vpos_latch == 1, "guest read did not latch vpos");
    CHECK(machine.zvb.spi.ram_rd.idx != 0, "guest read did not consume the SPI FIFO");

    zeal_deinit(&machine);
    if (failures == 0) printf("debug inspection: OK\n");
    return failures ? 1 : 0;
}
