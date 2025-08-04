/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "debugger_types.h"
#include "debugger_impl.h"


/* Breakpoint management */
bool debugger_set_breakpoint(dbg_t *dbg, hwaddr address);
bool debugger_clear_breakpoint(dbg_t *dbg, hwaddr address);
bool debugger_toggle_breakpoint(dbg_t *dbg, hwaddr address);
bool debugger_is_breakpoint_set(dbg_t *dbg, hwaddr address);
int debugger_get_breakpoints(dbg_t *dbg, hwaddr *bp, unsigned int size);

/* Watchpoint management */
bool debugger_add_watchpoint(dbg_t *dbg, watchpoint_t wp);
bool debugger_remove_watchpoint(dbg_t *dbg, hwaddr address);
bool debugger_is_watchpoint_set(dbg_t *dbg, hwaddr address);
int debugger_get_watchpoints(dbg_t *dbg, watchpoint_t* wps, unsigned int size);

/* Memory inspection */
void debugger_read_memory(dbg_t *dbg, hwaddr addr, int len, uint8_t *val);
void debugger_write_memory(dbg_t *dbg, hwaddr addr, int len, uint8_t *val);

/* Register access */
void debugger_get_registers(dbg_t *dbg, regs_t *regs);
void debugger_set_registers(dbg_t *dbg, regs_t *regs);

/* Execution control */
typedef void (*debugger_callback_t)(dbg_t *dbg);
void debugger_step(dbg_t *dbg);
void debugger_step_over(dbg_t *dbg);
void debugger_continue(dbg_t *dbg);
void debugger_pause(dbg_t *dbg);
void debugger_reset(dbg_t *dbg);
void debugger_breakpoint(dbg_t *dbg);
bool debugger_is_paused(dbg_t *dbg);

/* Event handling */
debug_event_t debugger_check_event(dbg_t *dbg);
void debugger_handle_event(dbg_t *dbg, debug_event_t event);

/* Symbol management */
bool debugger_load_symbols(dbg_t *dbg, const char *symbol_file);
const char* debugger_get_symbol(dbg_t *dbg, hwaddr address);
bool debugger_find_symbol(dbg_t *dbg, const char *symbol_name, hwaddr *address);

/* Disassembly */
// int debugger_disassemble(dbg_t *dbg, hwaddr address, char *buffer, int size);
int debugger_disassemble_address(dbg_t *dbg, hwaddr address, dbg_instr_t* instr);

/* Debugger initialization */
void debugger_init(dbg_t *dbg);
void debugger_deinit(dbg_t *dbg);

