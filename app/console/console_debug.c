/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "app/console/console.h"
#include "app/console/console_debug.h"
#include "utils/log.h"
#include "debugger/debugger.h"
#include "debugger/zeal_debugger.h"


/* Resolve an address argument: hex/dec number or a symbol name */
static bool console_debug_resolve_addr(zeal_t* machine, const char* str, hwaddr* addr)
{
    char* endptr = NULL;
    const unsigned long val = strtoul(str, &endptr, 0);
    if (endptr != str && *endptr == '\0') {
        *addr = (hwaddr) val;
        return true;
    }
    return debugger_find_symbol(&machine->dbg, str, addr);
}


/* Report where the machine stopped, with the disassembled instruction */
static void console_debug_report(zeal_t* machine)
{
    regs_t regs;
    debugger_get_registers(&machine->dbg, &regs);

    dbg_instr_t instr;
    if (debugger_disassemble_address(&machine->dbg, regs.pc, &instr) > 0) {
        log_printf("[DBG] Paused @ 0x%04x (cyc=%lu): %s\n", regs.pc, machine->cpu.cyc, instr.instruction);
    } else {
        log_printf("[DBG] Paused @ 0x%04x (cyc=%lu)\n", regs.pc, machine->cpu.cyc);
    }
}


void console_debug_continue(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    debugger_continue(&machine->dbg);
    zeal_debugger_run(machine, 0);
    console_debug_report(machine);
}


void console_debug_step(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    debugger_step(&machine->dbg);
    zeal_debugger_run(machine, 0);
    console_debug_report(machine);
}


void console_debug_step_over(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    debugger_step_over(&machine->dbg);
    zeal_debugger_run(machine, 0);
    console_debug_report(machine);
}


void console_debug_pause(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    debugger_pause(&machine->dbg);
    log_printf("[DBG] Paused\n");
}


void console_debug_bp(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: bp <addr|symbol>\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    if (debugger_set_breakpoint(&machine->dbg, addr)) {
        log_printf("[DBG] Breakpoint set @ 0x%04x\n", addr);
    } else {
        log_err_printf("[CONSOLE] Failed to set breakpoint @ 0x%04x\n", addr);
    }
}


void console_debug_bc(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: bc <addr|symbol>\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    if (debugger_clear_breakpoint(&machine->dbg, addr)) {
        log_printf("[DBG] Breakpoint cleared @ 0x%04x\n", addr);
    } else {
        log_printf("[DBG] No breakpoint @ 0x%04x\n", addr);
    }
}


void console_debug_bl(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    hwaddr bps[DBG_MAX_POINTS];
    const int count = debugger_get_breakpoints(&machine->dbg, bps, DBG_MAX_POINTS);
    if (count == 0) {
        log_printf("[DBG] No breakpoints\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        const char* sym = debugger_get_symbol(&machine->dbg, bps[i]);
        if (sym != NULL) {
            log_printf("[DBG] %2d: 0x%04x (%s)\n", i, bps[i], sym);
        } else {
            log_printf("[DBG] %2d: 0x%04x\n", i, bps[i]);
        }
    }
}


void console_debug_regs(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    regs_t regs;
    debugger_get_registers(&machine->dbg, &regs);
    log_printf("[DBG] PC=%04x SP=%04x\n", regs.pc, regs.sp);
    log_printf("[DBG] AF=%04x BC=%04x DE=%04x HL=%04x\n", regs.af, regs.bc, regs.de, regs.hl);
    log_printf("[DBG] IX=%04x IY=%04x I=%02x R=%02x\n", regs.ix, regs.iy, regs.i, regs.r);
}


void console_debug_rb(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: rb <addr> [count]\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    int count = 16;
    if (argc >= 3) {
        char* endptr = NULL;
        const long n = strtol(argv[2], &endptr, 0);
        if (endptr == argv[2] || *endptr != '\0' || n < 1 || n > 256) {
            log_err_printf("[CONSOLE] Invalid count: '%s'\n", argv[2]);
            return;
        }
        count = (int) n;
    }
    uint8_t buf[256];
    debugger_read_memory(&machine->dbg, addr, count, buf);
    for (int i = 0; i < count; i += 16) {
        log_printf("[DBG] %04x: ", (uint16_t)(addr + i));
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            log_printf("%02x ", buf[i + j]);
        }
        log_printf("\n");
    }
}


void console_debug_r16(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: r16 <addr>\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    uint8_t buf[2];
    debugger_read_memory(&machine->dbg, addr, 2, buf);
    log_printf("[DBG] 0x%04x: %04x\n", (uint16_t) addr, (unsigned int)(buf[0] | (buf[1] << 8)));
}


void console_debug_r32(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: r32 <addr>\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    uint8_t buf[4];
    debugger_read_memory(&machine->dbg, addr, 4, buf);
    const uint32_t v = buf[0] | (buf[1] << 8) | (buf[2] << 16) | ((uint32_t)buf[3] << 24);
    log_printf("[DBG] 0x%04x: %08x\n", (uint16_t) addr, (unsigned int) v);
}


void console_debug_wb(zeal_t* machine, int argc, char** argv)
{
    if (argc < 3) {
        log_err_printf("[CONSOLE] Usage: wb <addr> <byte> [byte...]\n");
        return;
    }
    hwaddr addr;
    if (!console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    uint8_t buf[256];
    int n = 0;
    for (int i = 2; i < argc; i++) {
        char* endptr = NULL;
        const unsigned long v = strtoul(argv[i], &endptr, 0);
        if (endptr == argv[i] || *endptr != '\0' || v > 0xff) {
            log_err_printf("[CONSOLE] Invalid byte: '%s'\n", argv[i]);
            return;
        }
        buf[n++] = (uint8_t) v;
    }
    debugger_write_memory(&machine->dbg, addr, n, buf);
    log_printf("[DBG] Wrote %d byte(s) @ 0x%04x\n", n, (uint16_t) addr);
}


void console_debug_disasm(zeal_t* machine, int argc, char** argv)
{
    hwaddr addr = machine->cpu.pc;
    if (argc >= 2 && !console_debug_resolve_addr(machine, argv[1], &addr)) {
        log_err_printf("[CONSOLE] Invalid address or symbol: '%s'\n", argv[1]);
        return;
    }
    int count = 8;
    if (argc >= 3) {
        char* endptr = NULL;
        const long n = strtol(argv[2], &endptr, 0);
        if (endptr == argv[2] || *endptr != '\0' || n < 1 || n > 64) {
            log_err_printf("[CONSOLE] Invalid count: '%s'\n", argv[2]);
            return;
        }
        count = (int) n;
    }
    for (int i = 0; i < count; i++) {
        dbg_instr_t instr;
        const int size = debugger_disassemble_address(&machine->dbg, addr, &instr);
        if (size <= 0) {
            log_printf("[DBG] %04x: <invalid>\n", (uint16_t) addr);
            break;
        }
        const char* sym = debugger_get_symbol(&machine->dbg, addr);
        if (sym != NULL) {
            log_printf("[DBG] %04x: <%s>\n", (uint16_t) addr, sym);
        }
        log_printf("[DBG] %04x: %s\n", (uint16_t) addr, instr.instruction);
        addr += size;
    }
}


void console_debug_mmu(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    dbg_mmu_t mmu;
    if (!debugger_custom(&machine->dbg, ZEAL_DBG_OP_GET_MMU, &mmu)) {
        log_err_printf("[CONSOLE] Failed to get MMU info\n");
        return;
    }
    for (int i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
        const dbg_mmu_entry_t* e = &mmu.entries[i];
        log_printf("[DBG] %04x -> %06x (%s)\n", e->virt_addr, (unsigned int) e->phys_addr,
                   e->device != NULL ? e->device : "?");
    }
}
