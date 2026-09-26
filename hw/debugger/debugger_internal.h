/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "debugger/api.h"
#include <stdbool.h>
#include <stdint.h>

#define DBG_SYM_COUNT 32

/* Callback types */
typedef int (*debugger_dis_op)(dbg_t *dbg, hwaddr address, dbg_instr_t *instr);
typedef void (*debugger_ctrl_op)(dbg_t *dbg);
typedef bool (*debugger_chk_op)(dbg_t *dbg);
typedef void (*debugger_breakpoint_callback)(dbg_t *dbg, hwaddr address);
typedef void (*debugger_watchpoint_callback)(dbg_t *dbg, hwaddr address, watchpoint_type_t type);
typedef bool (*debugger_alt_op)(dbg_t *dbg, int operation, void *arg);
typedef void (*debugger_regs_op)(dbg_t *dbg, regs_t *regs);
typedef int (*debugger_mem_op)(dbg_t *dbg, hwaddr addr, int len, uint8_t *val);

typedef struct {
    hwaddr addr;
    bool active;
    bool temporary; // True if it needs to be deleted when reached
} breakpoint_t;

typedef struct {
    const char *name;
    hwaddr addr;
} symbol_t;

/* List of array for the symbols */
typedef struct symbols_t {
    symbol_t array[DBG_SYM_COUNT];
    unsigned int count;
    struct symbols_t *next;
} symbols_t;

struct dbg_t {
    uint8_t watch_mask[65536];
    bool running; // should the emulator continue running?
    breakpoint_t breakpoints[DBG_MAX_POINTS];
    watchpoint_t watchpoints[DBG_MAX_POINTS];
    symbols_t symbols;

    debugger_dis_op disassemble_cb;
    debugger_ctrl_op pause_cb;
    debugger_ctrl_op continue_cb;
    debugger_ctrl_op reset_cb;
    debugger_ctrl_op step_cb;
    debugger_ctrl_op step_over_cb;
    debugger_ctrl_op breakpoint_cb;
    debugger_chk_op is_paused_cb;
    debugger_regs_op get_regs_cb;
    debugger_regs_op set_regs_cb;
    debugger_mem_op get_mem_cb;
    debugger_mem_op set_mem_cb;
    debugger_alt_op alt_op;

    dbg_render_stats_t render_stats;
    uint8_t *video_pixels;
    dbg_image_info_t video_info;
    uint32_t video_capacity;
    debugger_ctrl_op stop_cb;
    dbg_reason_t reason;
    uint64_t sequence;
    dbg_event_record_t events[64];
    bool stopped;
    void *arg;
};

void debugger_record(dbg_t *, dbg_reason_t, uint32_t, uint32_t);

void debugger_capture_video(dbg_t *);

void debugger_init(dbg_t *);
void debugger_deinit(dbg_t *);
