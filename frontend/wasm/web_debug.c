/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <emscripten.h>
#include "utils/helpers.h"
#include "web_debug.h"

enum web_debug_command {
    WEB_DEBUG_PAUSE = 1,
    WEB_DEBUG_CONTINUE,
    WEB_DEBUG_STEP,
    WEB_DEBUG_RESET,
};

enum web_debug_reason {
    WEB_DEBUG_STARTUP,
    WEB_DEBUG_RUNNING,
    WEB_DEBUG_PAUSED,
    WEB_DEBUG_STEPPED,
    WEB_DEBUG_RESETTING,
    WEB_DEBUG_BREAKPOINT,
    WEB_DEBUG_WATCHPOINT,
};

/* The JS entry point only sets the mailbox. CPU and device operations run on
 * the main Asyncify stack, which may already be suspended on browser I/O. */
static unsigned pending_command;
static bool available;
static bool step_pending;

// clang-format off
EM_JS(bool, web_debug_requested, (), { return Boolean(Module.debuggerEnabled); });

EM_JS(void, web_debug_state, (int reason, const uint16_t* regs, uint32_t hi, uint32_t lo), {
    const base = regs >>> 1;
    const [a, f, b, c, d, e, h, l, ix, iy, pc, sp, i, r] = HEAPU16.subarray(base, base + 14);
    const reasons = ['startup', 'running', 'pause', 'step', 'reset', 'breakpoint', 'watchpoint'];
    Module.onDebugState?.({
        paused: reason !== 1,
        reason: reasons[reason],
        tstates: (hi >>> 0) * 0x100000000 + (lo >>> 0),
        registers: {a, f, b, c, d, e, h, l, ix, iy, pc, sp, i, r},
    });
});

EM_JS(void, web_debug_complete, (int error), { Module.onDebugComplete?.(error); });
// clang-format on

static void publish_state(zeal_t* machine, enum web_debug_reason reason)
{
    regs_t regs;
    debugger_get_registers(&machine->dbg, &regs);
    const uint16_t values[] = { regs.a, regs.f, regs.b, regs.c, regs.d, regs.e,
        regs.h, regs.l, regs.ix, regs.iy, regs.pc, regs.sp, regs.i, regs.r };
    const uint64_t tstates = machine->cpu.clock.now_ns / TSTATE_NS;
    web_debug_state(reason, values, tstates >> 32, (uint32_t)tstates);
}

EMSCRIPTEN_KEEPALIVE
bool zeal_debug_request_web(unsigned command)
{
    if (!available || !command || pending_command || step_pending) {
        return false;
    }
    pending_command = command;
    return true;
}

void web_debug_start(zeal_t* machine)
{
    available = machine->dbg_enabled;
    if (available) {
        debugger_pause(&machine->dbg);
        publish_state(machine, WEB_DEBUG_STARTUP);
    }
}

bool web_debug_service(zeal_t* machine)
{
    const unsigned command = pending_command;
    pending_command = 0;
    if (!command) {
        return false;
    }

    int error = 0;
    bool redraw = false;
    switch (command) {
        case WEB_DEBUG_PAUSE:
            debugger_pause(&machine->dbg);
            publish_state(machine, WEB_DEBUG_PAUSED);
            break;
        case WEB_DEBUG_CONTINUE:
            debugger_continue(&machine->dbg);
            publish_state(machine, WEB_DEBUG_RUNNING);
            break;
        case WEB_DEBUG_STEP:
            if (machine->dbg_state == ST_PAUSED) {
                debugger_step(&machine->dbg);
                step_pending = true;
                return false;
            }
            error = EBUSY;
            break;
        case WEB_DEBUG_RESET:
            debugger_reset(&machine->dbg);
            publish_state(machine, WEB_DEBUG_RESETTING);
            redraw = true;
            break;
        default:
            error = EINVAL;
            break;
    }

    web_debug_complete(error);
    return redraw;
}

void web_debug_after_step(zeal_t* machine)
{
    enum web_debug_reason reason = WEB_DEBUG_STEPPED;
    if (machine->dbg.watch_stopped) {
        reason = WEB_DEBUG_WATCHPOINT;
    } else if (machine->dbg.breakpoint_stopped) {
        reason = WEB_DEBUG_BREAKPOINT;
    }
    publish_state(machine, reason);
    if (step_pending) {
        step_pending = false;
        web_debug_complete(0);
    }
}
