/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/host.h"
#include <stdbool.h>
#include "debugger/debugger_types.h"

#define DBG_MAX_POINTS  128
#define DBG_SYM_COUNT   32

/* Callback types */
typedef int  (*debugger_dis_op)(dbg_t *dbg, hwaddr address, dbg_instr_t* instr);
typedef void (*debugger_ctrl_op)(dbg_t *dbg);
typedef bool (*debugger_chk_op)(dbg_t *dbg);
typedef void (*debugger_breakpoint_callback)(dbg_t *dbg, hwaddr address);
typedef void (*debugger_watchpoint_callback)(dbg_t *dbg, hwaddr address, watchpoint_type_t type);
typedef bool (*debugger_alt_op)(dbg_t *dbg, int operation, void* arg);
typedef void (*debugger_regs_op)(dbg_t *dbg, regs_t *regs);
typedef int (*debugger_mem_op)(dbg_t *dbg, hwaddr addr, int len, uint8_t *val);

/* Create a pair of registers that can be accessed as bytes of a single 16-bit value */
#define REGISTER_PAIR(msb, lsb, pair) \
    union { \
        struct { \
            uint8_t lsb; \
            uint8_t msb; \
        }; \
        uint16_t pair; \
    }

/* CPU Register structure */
struct regs_t {
    uint16_t pc;
    uint16_t sp;

    REGISTER_PAIR(a, f, af);
    REGISTER_PAIR(b, c, bc);
    REGISTER_PAIR(d, e, de);
    REGISTER_PAIR(h, l, hl);

    /* Alternate register set */
    REGISTER_PAIR(a_, f_, af_);
    REGISTER_PAIR(b_, c_, bc_);
    REGISTER_PAIR(d_, e_, de_);
    REGISTER_PAIR(h_, l_, hl_);

    uint16_t ix, iy;
    REGISTER_PAIR(i, r, ir);
};


typedef struct {
    hwaddr            addr;
    watchpoint_type_t type;
} watchpoint_t;

typedef struct {
    hwaddr  addr;
    bool    active;     // Slot in use
    bool    temporary;  // True if it needs to be deleted when reached
    bool    disabled;   // Kept in the list but never stops execution
} breakpoint_t;

/* What a symbol refers to, from the map file section it lives in */
typedef enum {
    SYM_KIND_UNKNOWN = 0,   /* no section (equates, absolute values) */
    SYM_KIND_CODE,          /* code: functions and labels */
    SYM_KIND_DATA,          /* variables (data, bss) */
    SYM_KIND_CONST,         /* read-only data */
} symbol_kind_t;

typedef struct {
    const char* name;
    hwaddr      addr;
    uint8_t     kind;       /* symbol_kind_t */
    bool        local;      /* file-local label (z88dk maps only) */
} symbol_t;

/* List of array for the symbols */
typedef struct symbols_t {
    symbol_t        array[DBG_SYM_COUNT];
    unsigned int    count;
    struct symbols_t*      next;
} symbols_t;


struct dbg_t {
    const hw_host_t* host;
    char ed_fmt[2][64];
    instr_data_t ed_opcode;
    char ixiy_fmt[sizeof("bit 0,(i%c%+d)")];
    bool            running; // should the emulator continue running?
    breakpoint_t    breakpoints[DBG_MAX_POINTS];
    watchpoint_t    watchpoints[DBG_MAX_POINTS];
    symbols_t       symbols;

    debugger_dis_op  disassemble_cb;
    debugger_ctrl_op pause_cb;
    debugger_ctrl_op continue_cb;
    debugger_ctrl_op reset_cb;
    debugger_ctrl_op step_cb;
    debugger_ctrl_op step_over_cb;
    debugger_ctrl_op breakpoint_cb;
    debugger_chk_op  is_paused_cb;
    debugger_regs_op get_regs_cb;
    debugger_regs_op set_regs_cb;
    debugger_mem_op  get_mem_cb;
    debugger_mem_op  set_mem_cb;
    debugger_alt_op  alt_op;

    void*            arg;
};
