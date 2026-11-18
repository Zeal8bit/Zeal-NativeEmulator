/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execution trace and heuristic call stack (see debugger/zeal_trace.h).
 *
 * zeal_trace_before() fills the next ring slot in place from the CPU state
 * before z80_step(); zeal_trace_after() looks at the state after it to keep
 * the call stack and to log an interrupt accepted at the end of the step
 * (z80_step executes one instruction, then may accept an interrupt). */

#include <string.h>
#include "hw/zeal.h"
#include "debugger/zeal_trace.h"
#include "debugger/zeal_debugger.h"

#if CONFIG_TRACE_DEPTH > 0

/* Bytes as the CPU sees them, without side effects (flash status reads...) */
static inline uint8_t peek(const mmu_t* mmu, uint16_t virt_addr)
{
    const int idx = (virt_addr >> 14) & 0x3;
    const mmu_cached_page_t* vp = &mmu->vpages[idx];
    device_t* dev = vp->dev;
    if (!dev) return 0;
    const uint32_t off = (((uint32_t) mmu->pages[idx] << 14) | (virt_addr & (MMU_PAGE_SIZE - 1))) - vp->base_phys;
    return dev->mem_region.debug_read ? dev->mem_region.debug_read(dev, off)
                                      : dev->mem_region.read(dev, off);
}

static zeal_trace_entry_t* next_slot(zeal_trace_t* t)
{
    zeal_trace_entry_t* e = &t->entries[t->head];
    if (++t->head == CONFIG_TRACE_DEPTH) t->head = 0;
    if (t->count < CONFIG_TRACE_DEPTH) t->count++;
    t->serial++;
    return e;
}

static void fill_regs(zeal_trace_entry_t* e, z80* cpu)
{
    e->cycle = cpu->cyc;
    e->pc = cpu->pc;
    e->target = 0;
    e->af = (uint16_t) ((cpu->a << 8) | z80_get_f(cpu));
    e->bc = (uint16_t) ((cpu->b << 8) | cpu->c);
    e->de = (uint16_t) ((cpu->d << 8) | cpu->e);
    e->hl = (uint16_t) ((cpu->h << 8) | cpu->l);
    e->ix = cpu->ix;
    e->iy = cpu->iy;
    e->sp = cpu->sp;
    e->page = cpu->mmu.pages[cpu->pc >> 14];
}

static void record_event(zeal_t* machine, zeal_trace_kind_t kind, uint16_t target)
{
    zeal_trace_entry_t* e = next_slot(&machine->trace);
    fill_regs(e, &machine->cpu);
    memset(e->bytes, 0, sizeof(e->bytes));
    e->target = target;
    e->kind = (uint8_t) kind;
}

static void clear_stack(zeal_trace_t* t, bool truncated)
{
    t->depth = 0;
    t->segment = 0;
    t->truncated = truncated;
}

static void push_frame(zeal_trace_t* t, zeal_frame_kind_t kind, uint16_t call_pc, uint16_t ret,
                       uint16_t target, uint16_t sp)
{
    if (t->depth == ZEAL_CALLSTACK_DEPTH) {
        /* Keep the innermost frames: they are the ones being looked at */
        memmove(&t->frames[0], &t->frames[1], sizeof(t->frames[0]) * (ZEAL_CALLSTACK_DEPTH - 1));
        t->depth--;
        t->truncated = true;
    }
    t->frames[t->depth++] = (zeal_frame_t) {
        .call_pc = call_pc, .ret = ret, .target = target, .sp = sp,
        .kind = (uint8_t) kind, .segment = t->segment,
    };
}

/* A frame of the current stack is gone once SP moved above the slot of its
 * return address (RET, POP, SP arithmetic...). The signed distance keeps this
 * right when the stack wraps at $0000/$FFFF. Frames of an outer stack are
 * left alone: SP says nothing about them. */
static void prune(zeal_trace_t* t, uint16_t sp)
{
    while (t->depth > 0) {
        const zeal_frame_t* f = &t->frames[t->depth - 1];
        if (f->segment != t->segment || (int16_t) (uint16_t) (sp - f->sp) <= 0) break;
        t->depth--;
    }
}

/* A return popped `ret` from `slot`: if a frame matches both, everything
 * above it returned too (whatever stack it was on). */
static bool unwind_to(zeal_trace_t* t, uint16_t slot, uint16_t ret)
{
    for (int i = t->depth - 1; i >= 0; i--) {
        if (t->frames[i].sp == slot && t->frames[i].ret == ret) {
            t->segment = t->frames[i].segment;
            t->depth = i;
            return true;
        }
    }
    return false;
}

/* SP was loaded (LD SP,nn / LD SP,HL / LD SP,IX / LD SP,(nn)). Back to the
 * SP saved when a stack was entered: leave that stack (its frames are
 * done). Otherwise a new stack starts (OS kernel stack, task switch, a new
 * program): the frames below stay, as the calls that led there. */
static void switch_stack(zeal_trace_t* t, uint16_t old_sp, uint16_t new_sp)
{
    for (int s = t->segment; s > 0; s--) {
        if (t->segment_sp[s - 1] == new_sp) {
            t->segment = (uint8_t) (s - 1);
            while (t->depth > 0 && t->frames[t->depth - 1].segment > t->segment) t->depth--;
            return;
        }
    }
    if (t->segment < ZEAL_STACK_SEGMENTS) {
        t->segment_sp[t->segment++] = old_sp;
    } else {
        /* Too many nested stacks: reuse the innermost one */
        t->segment_sp[ZEAL_STACK_SEGMENTS - 1] = old_sp;
        while (t->depth > 0 && t->frames[t->depth - 1].segment == t->segment) t->depth--;
    }
}

static bool loads_sp(const uint8_t* b)
{
    return b[0] == 0x31 || b[0] == 0xF9 ||                      /* LD SP,nn / LD SP,HL */
           ((b[0] == 0xDD || b[0] == 0xFD) && b[1] == 0xF9) ||   /* LD SP,IX / LD SP,IY */
           (b[0] == 0xED && b[1] == 0x7B);                      /* LD SP,(nn) */
}

static bool is_return(const uint8_t* b)
{
    return b[0] == 0xC9 || (b[0] & 0xC7) == 0xC0 ||             /* RET / RET cc */
           (b[0] == 0xED && (b[1] & 0xC7) == 0x45);             /* RETN / RETI */
}

void zeal_trace_before(zeal_t* machine)
{
    zeal_trace_t* t = &machine->trace;
    z80* cpu = &machine->cpu;
    t->pending = -1;
    if ((uint64_t) cpu->cyc != t->last_cycle) {
        /* Code ran without the trace (debugger or trace switched off) */
        record_event(machine, ZEAL_TRACE_GAP, 0);
        clear_stack(t, true);
    }
    t->last_ints = cpu->int_count;
    /* DMA owns the bus (no opcode executes) or HALT repeats: nothing new */
    if (cpu->bus_requested || cpu->halted) return;
    t->pending = (int32_t) t->head;
    zeal_trace_entry_t* e = next_slot(t);
    fill_regs(e, cpu);
    uint16_t pc = cpu->pc;
    uint8_t op = peek(&cpu->mmu, pc);
    while ((op == 0xDD || op == 0xFD) && e->skipped_prefixes < UINT16_MAX) {
        const uint8_t next = peek(&cpu->mmu, (uint16_t) (pc + 1));
        if (zeal_debugger_uses_index(next)) break;
        e->skipped_prefixes++;
        pc++;
        op = next;
    }
    e->bytes[0] = op;
    e->bytes[1] = peek(&cpu->mmu, (uint16_t) (pc + 1));
    e->bytes[2] = peek(&cpu->mmu, (uint16_t) (pc + 2));
    e->bytes[3] = peek(&cpu->mmu, (uint16_t) (pc + 3));
    e->kind = ZEAL_TRACE_INSTR;
}

void zeal_trace_after(zeal_t* machine)
{
    zeal_trace_t* t = &machine->trace;
    z80* cpu = &machine->cpu;
    t->last_cycle = cpu->cyc;

    /* State right after the instruction: an interrupt accepted at the end of
     * the step pushed the PC the instruction left (except a mode 0 opcode
     * that is not an RST, which pushes nothing). */
    const bool interrupted = cpu->int_count != t->last_ints;
    const bool int_pushed = interrupted &&
        (cpu->int_kind != Z80_INT_IM0 || (cpu->int_data & 0xC7) == 0xC7);
    uint16_t sp = cpu->sp;
    uint16_t pc = cpu->pc;
    if (int_pushed) {
        pc = (uint16_t) (peek(&cpu->mmu, sp) | (peek(&cpu->mmu, (uint16_t) (sp + 1)) << 8));
        sp = (uint16_t) (sp + 2);
    }

    if (t->pending >= 0) {
        const zeal_trace_entry_t* e = &t->entries[t->pending];
        const uint8_t op = e->bytes[0];
        const bool pushed = sp == (uint16_t) (e->sp - 2);
        if (op == 0xCD || (op & 0xC7) == 0xC4) {
            /* CALL nn / CALL cc,nn (taken when the return address was pushed) */
            prune(t, sp);
            if (pushed) push_frame(t, ZEAL_FRAME_CALL, e->pc, (uint16_t) (e->pc + e->skipped_prefixes + 3), pc, sp);
        } else if ((op & 0xC7) == 0xC7) {
            prune(t, sp);
            if (pushed) push_frame(t, ZEAL_FRAME_RST, e->pc, (uint16_t) (e->pc + e->skipped_prefixes + 1), pc, sp);
        } else if (loads_sp(e->bytes)) {
            if (sp != e->sp) switch_stack(t, e->sp, sp);
        } else if (is_return(e->bytes) && sp == (uint16_t) (e->sp + 2)) {
            if (!unwind_to(t, e->sp, pc)) prune(t, sp);
        } else {
            prune(t, sp);
        }
        t->pending = -1;
    } else {
        prune(t, sp);
    }

    if (interrupted) {
        static const uint8_t kinds[] = { ZEAL_TRACE_NMI, ZEAL_TRACE_IM0, ZEAL_TRACE_IM1, ZEAL_TRACE_IM2 };
        const uint8_t kind = kinds[cpu->int_kind & 3];
        zeal_trace_entry_t* e = next_slot(t);
        fill_regs(e, cpu);
        /* Registers as the handler sees them on entry, at the interrupted PC */
        e->pc = pc;
        e->page = cpu->mmu.pages[pc >> 14];
        e->sp = cpu->sp;
        e->target = cpu->pc;
        memset(e->bytes, 0, sizeof(e->bytes));
        e->kind = kind;
        if (int_pushed) {
            push_frame(t, kind == ZEAL_TRACE_NMI ? ZEAL_FRAME_NMI : ZEAL_FRAME_INT,
                       pc, pc, cpu->pc, cpu->sp);
        }
        t->last_ints = cpu->int_count;
    }
}

void zeal_trace_on_reset(zeal_t* machine)
{
    zeal_trace_t* t = &machine->trace;
    clear_stack(t, false);
    t->pending = -1;
    t->last_ints = machine->cpu.int_count;
    if (t->enabled && machine->dbg_enabled) record_event(machine, ZEAL_TRACE_RESET, 0);
    t->last_cycle = machine->cpu.cyc;
}

unsigned zeal_trace_count(const zeal_t* machine)
{
    return machine ? machine->trace.count : 0;
}

bool zeal_trace_get(const zeal_t* machine, unsigned age, zeal_trace_entry_t* out)
{
    if (machine == NULL || out == NULL) return false;
    const zeal_trace_t* t = &machine->trace;
    if (age >= t->count) return false;
    const unsigned idx = (t->head + CONFIG_TRACE_DEPTH - 1 - age) % CONFIG_TRACE_DEPTH;
    *out = t->entries[idx];
    return true;
}

uint64_t zeal_trace_serial(const zeal_t* machine)
{
    return machine ? machine->trace.serial : 0;
}

void zeal_trace_clear(zeal_t* machine)
{
    if (machine == NULL) return;
    zeal_trace_t* t = &machine->trace;
    t->head = 0;
    t->count = 0;
    t->serial++;
    t->pending = -1;
    /* Keep the call stack: it describes the live machine, not the log */
}

void zeal_trace_set_enabled(zeal_t* machine, bool enabled)
{
    if (machine) machine->trace.enabled = enabled;
}

bool zeal_trace_enabled(const zeal_t* machine)
{
    return machine && machine->trace.enabled;
}

int zeal_callstack_get(const zeal_t* machine, zeal_frame_t* out, int max)
{
    if (machine == NULL || out == NULL) return 0;
    const zeal_trace_t* t = &machine->trace;
    const int n = t->depth < max ? t->depth : max;
    /* The innermost frames when `out` is too small */
    memcpy(out, &t->frames[t->depth - n], sizeof(*out) * (size_t) n);
    return n;
}

bool zeal_callstack_truncated(const zeal_t* machine)
{
    return machine && machine->trace.truncated;
}

#else /* CONFIG_TRACE_DEPTH == 0: recording compiled out */

void zeal_trace_before(zeal_t* machine) { (void) machine; }
void zeal_trace_after(zeal_t* machine) { (void) machine; }
void zeal_trace_on_reset(zeal_t* machine) { (void) machine; }
unsigned zeal_trace_count(const zeal_t* machine) { (void) machine; return 0; }
bool zeal_trace_get(const zeal_t* machine, unsigned age, zeal_trace_entry_t* out)
{
    (void) machine;
    (void) age;
    (void) out;
    return false;
}
uint64_t zeal_trace_serial(const zeal_t* machine) { (void) machine; return 0; }
void zeal_trace_clear(zeal_t* machine) { (void) machine; }
void zeal_trace_set_enabled(zeal_t* machine, bool enabled) { (void) machine; (void) enabled; }
bool zeal_trace_enabled(const zeal_t* machine) { (void) machine; return false; }
int zeal_callstack_get(const zeal_t* machine, zeal_frame_t* out, int max)
{
    (void) machine;
    (void) out;
    (void) max;
    return 0;
}
bool zeal_callstack_truncated(const zeal_t* machine) { (void) machine; return false; }

#endif

unsigned zeal_trace_capacity(void)
{
    return CONFIG_TRACE_DEPTH;
}
