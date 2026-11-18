/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execution trace ("how did I get here") and heuristic call stack.
 *
 * While the debugger is enabled, zeal_step() records every executed
 * instruction (PC, opcode bytes, registers before execution, cycle count)
 * into a fixed ring of CONFIG_TRACE_DEPTH entries written in place, plus
 * interrupt acceptances and resets as events. The same hook maintains a
 * call stack derived from CALL/RST/interrupt entries. A return that pops a
 * frame's return address from its slot unwinds to it; otherwise frames are
 * dropped once SP moves above the slot holding their return address (POP,
 * SP arithmetic). Loading SP (LD SP,...) starts a nested stack, such as the
 * OS kernel stack of a syscall, keeping the frames that led there; loading
 * back the SP saved at that switch leaves it.
 *
 * CONFIG_TRACE_DEPTH is a compile-time value (meson -Dtrace_depth=N); 0
 * compiles the recording out of the step loop. The query API below exists
 * whenever the debugger is compiled in and reports an empty trace then.
 *
 * Plain C types only: included by the C++ UI through its bridge. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef CONFIG_TRACE_DEPTH
#define CONFIG_TRACE_DEPTH 4096
#endif

/* Deepest call stack kept; the outermost frames are dropped past this. */
#define ZEAL_CALLSTACK_DEPTH 64
/* Nested stacks tracked (e.g. user stack -> kernel stack -> ...) */
#define ZEAL_STACK_SEGMENTS 8

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZEAL_TRACE_INSTR = 0,   /* an executed instruction */
    ZEAL_TRACE_NMI,         /* NMI accepted: pc = return address, target = $0066 */
    ZEAL_TRACE_IM0,         /* maskable interrupt accepted, mode 0 */
    ZEAL_TRACE_IM1,         /* mode 1: target = $0038 */
    ZEAL_TRACE_IM2,         /* mode 2: target read from the vector table */
    ZEAL_TRACE_RESET,       /* CPU reset (call stack cleared) */
    ZEAL_TRACE_GAP,         /* instructions ran untraced (debugger or trace off) */
} zeal_trace_kind_t;

/* 32 bytes. Registers are the values before the instruction executed (for
 * events: when the event happened). */
typedef struct {
    uint64_t cycle;         /* CPU T-state counter (since reset) */
    uint16_t pc;            /* instruction address; events: interrupted PC */
    union {
        uint16_t target;            /* events: handler address */
        uint16_t skipped_prefixes;  /* instructions: ignored DD/FD bytes before bytes[] */
    };
    uint16_t af, bc, de, hl, ix, iy, sp;
    uint8_t  bytes[4];      /* opcode bytes at pc + skipped_prefixes */
    uint8_t  kind;          /* zeal_trace_kind_t */
    uint8_t  page;          /* MMU page register of pc's 16K bank */
} zeal_trace_entry_t;

typedef enum {
    ZEAL_FRAME_CALL = 0,
    ZEAL_FRAME_RST,
    ZEAL_FRAME_INT,         /* maskable interrupt */
    ZEAL_FRAME_NMI,
} zeal_frame_kind_t;

typedef struct {
    uint16_t call_pc;       /* CALL/RST address; interrupts: interrupted PC */
    uint16_t ret;           /* return address */
    uint16_t target;        /* callee / handler entry */
    uint16_t sp;            /* stack slot holding the return address */
    uint8_t  kind;          /* zeal_frame_kind_t */
    uint8_t  segment;       /* stack it lives on: 0 = first, +1 per stack switch */
} zeal_frame_t;

typedef struct {
#if CONFIG_TRACE_DEPTH > 0
    zeal_trace_entry_t entries[CONFIG_TRACE_DEPTH];
#endif
    uint32_t head;          /* next slot to write */
    uint32_t count;         /* valid entries (<= CONFIG_TRACE_DEPTH) */
    uint64_t serial;        /* entries ever written: changes on every record */
    bool     enabled;       /* runtime switch (on by default) */

    zeal_frame_t frames[ZEAL_CALLSTACK_DEPTH];  /* [0] = outermost */
    int      depth;
    uint8_t  segment;       /* current stack */
    uint16_t segment_sp[ZEAL_STACK_SEGMENTS];   /* SP when stack n+1 was entered */
    bool     truncated;     /* outer frames were dropped (overflow or gap) */

    /* Step bookkeeping */
    uint64_t last_cycle;    /* cycle counter after the last traced step */
    uint32_t last_ints;     /* cpu.int_count after the last traced step */
    int32_t  pending;       /* slot recorded by zeal_trace_before, or -1 */
} zeal_trace_t;

struct zeal_t;

/** Number of entries the ring holds (CONFIG_TRACE_DEPTH). */
unsigned zeal_trace_capacity(void);
/** Valid entries recorded so far, at most the capacity. */
unsigned zeal_trace_count(const struct zeal_t* machine);
/** Entry `age` (0 = most recent). False when out of range. */
bool     zeal_trace_get(const struct zeal_t* machine, unsigned age, zeal_trace_entry_t* out);
/** Number of entries ever written: a cheap change detector. */
uint64_t zeal_trace_serial(const struct zeal_t* machine);
/** Forget the recorded entries and the call stack. */
void     zeal_trace_clear(struct zeal_t* machine);
void     zeal_trace_set_enabled(struct zeal_t* machine, bool enabled);
bool     zeal_trace_enabled(const struct zeal_t* machine);

/** Call stack frames, outermost first; returns the number copied. */
int      zeal_callstack_get(const struct zeal_t* machine, zeal_frame_t* out, int max);
/** True when outer frames are missing (stack overflowed, or tracing resumed mid-call). */
bool     zeal_callstack_truncated(const struct zeal_t* machine);

/* Step loop hooks (hw/zeal.c) */
void     zeal_trace_before(struct zeal_t* machine);
void     zeal_trace_after(struct zeal_t* machine);
void     zeal_trace_on_reset(struct zeal_t* machine);

#ifdef __cplusplus
}
#endif
