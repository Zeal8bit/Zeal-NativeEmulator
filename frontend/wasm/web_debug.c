/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include <emscripten.h>
#include "debugger/zeal_debugger.h"
#include "frontend/common/symbols.h"
#include "utils/helpers.h"
#include "web_debug.h"

enum web_debug_command {
    WEB_DEBUG_PAUSE = 1,
    WEB_DEBUG_CONTINUE,
    WEB_DEBUG_STEP,
    WEB_DEBUG_RESET,
    WEB_DEBUG_STEP_OVER,
    WEB_DEBUG_SET_BREAKPOINT,
    WEB_DEBUG_REMOVE_BREAKPOINT,
    WEB_DEBUG_DISASSEMBLE,
    WEB_DEBUG_LOAD_SYMBOLS,
    WEB_DEBUG_FIND_SYMBOL,
    WEB_DEBUG_INSPECT_MEMORY,
    WEB_DEBUG_WRITE_MEMORY,
};

enum web_debug_reason {
    WEB_DEBUG_STARTUP,
    WEB_DEBUG_RUNNING,
    WEB_DEBUG_PAUSED,
    WEB_DEBUG_STEPPED,
    WEB_DEBUG_RESETTING,
    WEB_DEBUG_BREAKPOINT,
    WEB_DEBUG_WATCHPOINT,
    WEB_DEBUG_STEPPED_OVER,
};

/* The JS entry point only sets the mailbox. CPU and device operations run on
 * the main Asyncify stack, which may already be suspended on browser I/O. */
typedef struct {
    unsigned command, address, count;
    const uint8_t* data;
    unsigned length;
} web_debug_request_t;

static web_debug_request_t pending;
static bool available;
static bool step_pending;
static bool stepping_over;
static bool before_first_run;
static uint8_t memory_bytes[UINT16_MAX + 1u];

// clang-format off
EM_JS(bool, web_debug_requested, (), { return Boolean(Module.debuggerEnabled); });

EM_JS(void, web_debug_state, (int reason, const uint16_t* regs, uint32_t hi, uint32_t lo), {
    const base = regs >>> 1;
    const [a, f, b, c, d, e, h, l, ix, iy, pc, sp, i, r] = HEAPU16.subarray(base, base + 14);
    const reasons = ['startup', 'running', 'pause', 'step', 'reset', 'breakpoint', 'watchpoint', 'step-over'];
    Module.onDebugState?.({
        paused: reason !== 1,
        reason: reasons[reason],
        tstates: (hi >>> 0) * 0x100000000 + (lo >>> 0),
        registers: {a, f, b, c, d, e, h, l, ix, iy, pc, sp, i, r},
    });
});

EM_JS(void, web_debug_complete, (int error), { Module.onDebugComplete?.(error); });

EM_JS(void, web_debug_disassembly_start, (), { Module.debugDisassembly = []; });
EM_JS(void, web_debug_disassembly_row,
      (unsigned address, const char* text, const char* label, const uint8_t* opcodes, unsigned size), {
    Module.debugDisassembly.push({
        address,
        instruction: UTF8ToString(text, 64),
        label: UTF8ToString(label, 64),
        bytes: HEAPU8.slice(opcodes, opcodes + size),
    });
});
EM_JS(void, web_debug_disassembly_complete, (), {
    const output = Module.debugDisassembly;
    delete Module.debugDisassembly;
    Module.onDebugComplete?.(0, output);
});
EM_JS(void, web_debug_symbol_complete, (int address), {
    Module.onDebugComplete?.(0, address >= 0 ? address : null);
});
EM_JS(void, web_debug_memory_complete,
      (const uint8_t* data, unsigned count, const uint32_t* bases,
       const char* const* devices, unsigned pages), {
    const mmu = [];
    for (let index = 0; index < pages; index++) {
        const device = HEAPU32[(devices >>> 2) + index];
        mmu.push({
            logical: index * 0x4000,
            physical: HEAPU32[(bases >>> 2) + index],
            device: device ? UTF8ToString(device) : null,
        });
    }
    Module.onDebugComplete?.(0, {bytes: HEAPU8.slice(data, data + count), mmu});
});
EM_JS(void, web_debug_write_complete, (const uint8_t* data, unsigned count), {
    Module.onDebugComplete?.(0, HEAPU8.slice(data, data + count));
});
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
bool zeal_debug_request_web(unsigned command, unsigned address, unsigned count,
                            const uint8_t* data, unsigned length)
{
    if (!available || !command || pending.command || step_pending) {
        return false;
    }
    pending = (web_debug_request_t){ command, address, count, data, length };
    return true;
}

static void disassemble(dbg_t* dbg, uint16_t address, unsigned count)
{
    web_debug_disassembly_start();
    for (unsigned i = 0; i < count; i++) {
        dbg_instr_t instruction = {0};
        int size = debugger_disassemble_address(dbg, address, &instruction);
        if (size < 1 || size > (int)sizeof(instruction.opcodes)) {
            size = 1;
            debugger_read_memory(dbg, address, size, instruction.opcodes);
        }
        web_debug_disassembly_row(address, instruction.instruction, instruction.label,
                                  instruction.opcodes, size);
        address = (uint16_t)(address + size);
    }
    web_debug_disassembly_complete();
}

static bool load_symbols(dbg_t* dbg, const uint8_t* data, unsigned length)
{
    if (length == 0) {
        debugger_clear_symbols(dbg);
        return true;
    }
    FILE* file = fmemopen((void*)data, length, "r");
    if (!file) {
        return false;
    }
    const bool loaded = debugger_load_symbols_stream(dbg, file, "browser map", false);
    fclose(file);
    return loaded;
}

static bool inspect_memory(dbg_t* dbg, unsigned address, unsigned count)
{
    dbg_mmu_t mmu;
    if (!debugger_custom(dbg, ZEAL_DBG_OP_GET_MMU, &mmu)) {
        return false;
    }
    uint32_t bases[ZEAL_DBG_MMU_PAGES];
    const char* devices[ZEAL_DBG_MMU_PAGES];
    for (unsigned i = 0; i < ZEAL_DBG_MMU_PAGES; i++) {
        bases[i] = mmu.entries[i].phys_addr;
        devices[i] = mmu.entries[i].device;
    }
    if (count) {
        debugger_read_memory(dbg, address, (int)count, memory_bytes);
    }
    web_debug_memory_complete(memory_bytes, count, bases, devices, ZEAL_DBG_MMU_PAGES);
    return true;
}

void web_debug_start(zeal_t* machine)
{
    available = machine->dbg_enabled;
    if (available) {
        debugger_pause(&machine->dbg);
        before_first_run = true;
#if CONFIG_TRACE_DEPTH > 0
        machine->trace.enabled = false;
#endif
        publish_state(machine, WEB_DEBUG_STARTUP);
    }
}

bool web_debug_service(zeal_t* machine)
{
    const web_debug_request_t request = pending;
    pending.command = 0;
    if (!request.command) {
        return false;
    }

    int error = 0;
    bool redraw = false;
    dbg_t* dbg = &machine->dbg;
    switch (request.command) {
        case WEB_DEBUG_PAUSE:
            stepping_over = false;
            debugger_pause(dbg);
            publish_state(machine, WEB_DEBUG_PAUSED);
            break;
        case WEB_DEBUG_CONTINUE:
            if (before_first_run && debugger_is_breakpoint_enabled(dbg, machine->cpu.pc)) {
                publish_state(machine, WEB_DEBUG_BREAKPOINT);
            } else {
                debugger_continue(dbg);
                publish_state(machine, WEB_DEBUG_RUNNING);
            }
            before_first_run = false;
            break;
        case WEB_DEBUG_STEP:
            if (machine->dbg_state == ST_PAUSED) {
                debugger_step(dbg);
                before_first_run = false;
                step_pending = true;
                return false;
            }
            error = EBUSY;
            break;
        case WEB_DEBUG_RESET:
            stepping_over = false;
            debugger_reset(dbg);
            publish_state(machine, WEB_DEBUG_RESETTING);
            redraw = true;
            break;
        case WEB_DEBUG_STEP_OVER:
            if (machine->dbg_state != ST_PAUSED) {
                error = EBUSY;
                break;
            }
            before_first_run = false;
            stepping_over = true;
            debugger_step_over(dbg);
            publish_state(machine, WEB_DEBUG_RUNNING);
            break;
        case WEB_DEBUG_SET_BREAKPOINT:
            if (request.address > UINT16_MAX || request.count > 1) {
                error = EINVAL;
                break;
            }
            if (!debugger_has_breakpoint(dbg, request.address) &&
                !debugger_set_breakpoint(dbg, request.address)) {
                error = ENOSPC;
                break;
            }
            debugger_enable_breakpoint(dbg, request.address, request.count != 0);
            break;
        case WEB_DEBUG_REMOVE_BREAKPOINT:
            if (request.address > UINT16_MAX) {
                error = EINVAL;
                break;
            }
            debugger_clear_breakpoint(dbg, request.address);
            break;
        case WEB_DEBUG_DISASSEMBLE:
            if (request.address > UINT16_MAX || request.count == 0 || request.count > 256) {
                error = EINVAL;
                break;
            }
            disassemble(dbg, (uint16_t)request.address, request.count);
            return false;
        case WEB_DEBUG_LOAD_SYMBOLS:
            if ((!request.data && request.length) || request.length > 4 * 1024 * 1024 ||
                !load_symbols(dbg, request.data, request.length)) {
                error = EINVAL;
            }
            break;
        case WEB_DEBUG_FIND_SYMBOL: {
            hwaddr address;
            if (!request.data || request.length == 0 || request.length > 256 ||
                request.data[request.length - 1] != 0) {
                error = EINVAL;
                break;
            }
            const bool found = debugger_find_symbol(dbg, (const char*)request.data, &address);
            web_debug_symbol_complete(found && address <= UINT16_MAX ? (int)address : -1);
            return false;
        }
        case WEB_DEBUG_INSPECT_MEMORY:
            if (request.address > UINT16_MAX || request.count > sizeof(memory_bytes)) {
                error = EINVAL;
                break;
            }
            if (!inspect_memory(dbg, request.address, request.count)) {
                error = EIO;
                break;
            }
            return false;
        case WEB_DEBUG_WRITE_MEMORY:
            if (machine->dbg_state != ST_PAUSED) {
                error = EBUSY;
                break;
            }
            if (request.address > UINT16_MAX || !request.data ||
                request.length == 0 || request.length > sizeof(memory_bytes)) {
                error = EINVAL;
                break;
            }
            memcpy(memory_bytes, request.data, request.length);
            debugger_write_memory(dbg, request.address, (int)request.length, memory_bytes);
            debugger_read_memory(dbg, request.address, (int)request.length, memory_bytes);
            web_debug_write_complete(memory_bytes, request.length);
            return true;
        default:
            error = EINVAL;
            break;
    }

    web_debug_complete(error);
    return redraw;
}

void web_debug_after_step(zeal_t* machine)
{
    enum web_debug_reason reason = stepping_over ? WEB_DEBUG_STEPPED_OVER : WEB_DEBUG_STEPPED;
    stepping_over = false;
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
