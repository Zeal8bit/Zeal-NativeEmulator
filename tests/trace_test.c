/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execution trace and call stack test (meson test trace).
 *
 *   trace_test                      run the checks on a small program
 *   trace_test --bench ROM [MT]     time MT million T-states (default 500) of
 *                                   ROM: debugger off, trace off, trace on */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hw/zeal.h"

static int s_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            s_failures++;                                                    \
        }                                                                    \
    } while (0)

static const hw_host_t s_host = { 0 };
static zeal_t s_machine;

static void poke(uint16_t addr, const uint8_t* bytes, size_t n)
{
    memcpy(&s_machine.rom.data[addr], bytes, n);
}

static void load_program(void)
{
    static const uint8_t jp_main[] = { 0xC3, 0x00, 0x01 };            /* jp $0100 */
    static const uint8_t rst10[] = { 0xC9 };                          /* ret */
    static const uint8_t im1[] = { 0xFB, 0xED, 0x4D };                /* ei; reti */
    static const uint8_t nmi[] = { 0xED, 0x45 };                      /* retn */
    static const uint8_t main_code[] = {
        0x3E, 0x23,         /* 0100 ld a,$23   (RAM in the top 16K bank) */
        0xD3, 0xF3,         /* 0102 out ($f3),a */
        0x31, 0x00, 0x00,   /* 0104 ld sp,$0000 */
        0xED, 0x56,         /* 0107 im 1 */
        0xFB,               /* 0109 ei */
        0xCD, 0x00, 0x02,   /* 010A call $0200 */
        0xD7,               /* 010D rst $10 */
        0x00,               /* 010E nop */
        0x18, 0xFE,         /* 010F jr $ */
    };
    static const uint8_t func_a[] = { 0xCD, 0x00, 0x03, 0xC9 };       /* 0200 call $0300; ret */
    static const uint8_t func_b[] = {
        0x00,                   /* 0300 nop */
        0x00,                   /* 0301 nop */
        0xED, 0x73, 0x00, 0xE0, /* 0302 ld ($E000),sp   (switch to another stack, */
        0x31, 0x00, 0xF0,       /* 0306 ld sp,$F000      like a syscall) */
        0xCD, 0x00, 0x04,       /* 0309 call $0400 */
        0xED, 0x7B, 0x00, 0xE0, /* 030C ld sp,($E000) */
        0xC9,                   /* 0310 ret */
    };
    static const uint8_t func_c[] = { 0x00, 0xC9 };                   /* 0400 nop; ret */
    poke(0x0000, jp_main, sizeof(jp_main));
    poke(0x0010, rst10, sizeof(rst10));
    poke(0x0038, im1, sizeof(im1));
    poke(0x0066, nmi, sizeof(nmi));
    poke(0x0100, main_code, sizeof(main_code));
    poke(0x0200, func_a, sizeof(func_a));
    poke(0x0300, func_b, sizeof(func_b));
    poke(0x0400, func_c, sizeof(func_c));
}

static void step(void)
{
    s_machine.dbg_state = ST_RUNNING;
    zeal_step(&s_machine, true);
}

static void run_to(uint16_t pc)
{
    for (int i = 0; i < 1000 && s_machine.cpu.pc != pc; i++) step();
    CHECK(s_machine.cpu.pc == pc);
}

static int stack(zeal_frame_t* frames)
{
    return zeal_callstack_get(&s_machine, frames, ZEAL_CALLSTACK_DEPTH);
}

static zeal_trace_entry_t entry(unsigned age)
{
    zeal_trace_entry_t e;
    memset(&e, 0, sizeof(e));
    CHECK(zeal_trace_get(&s_machine, age, &e));
    return e;
}

static void check_frame(const zeal_frame_t* f, int kind, uint16_t call_pc, uint16_t ret, uint16_t target, uint16_t sp)
{
    CHECK(f->kind == kind);
    CHECK(f->call_pc == call_pc);
    CHECK(f->ret == ret);
    CHECK(f->target == target);
    CHECK(f->sp == sp);
}

static int init_machine(bool frames)
{
    const zeal_options_t options = { .host = &s_host, .frames = frames, .debugger = true };
    if (zeal_init(&s_machine, &options) != 0) {
        fprintf(stderr, "zeal_init failed\n");
        return 1;
    }
    return 0;
}

static int run_checks(void)
{
    if (init_machine(false)) return 1;
    load_program();
    zeal_reset(&s_machine);
    zeal_frame_t f[ZEAL_CALLSTACK_DEPTH];

    CHECK(sizeof(zeal_trace_entry_t) == 32);
    CHECK(zeal_trace_capacity() == CONFIG_TRACE_DEPTH);
    /* Reset event first */
    CHECK(zeal_trace_count(&s_machine) == 1);
    CHECK(entry(0).kind == ZEAL_TRACE_RESET);

    /* Two nested calls */
    run_to(0x0300);
    CHECK(stack(f) == 2);
    check_frame(&f[0], ZEAL_FRAME_CALL, 0x010A, 0x010D, 0x0200, 0xFFFE);
    check_frame(&f[1], ZEAL_FRAME_CALL, 0x0200, 0x0203, 0x0300, 0xFFFC);
    /* Entries in order, newest first: call $0300 at $0200, call $0200, ei, im 1, ld sp, out, ld a, jp */
    {
        static const uint16_t pcs[] = { 0x0200, 0x010A, 0x0109, 0x0107, 0x0104, 0x0102, 0x0100, 0x0000 };
        for (unsigned i = 0; i < CONFIG_TRACE_DEPTH && i < sizeof(pcs) / sizeof(pcs[0]); i++) {
            const zeal_trace_entry_t e = entry(i);
            CHECK(e.kind == ZEAL_TRACE_INSTR);
            CHECK(e.pc == pcs[i]);
            if (i > 0) CHECK(e.cycle < entry(i - 1).cycle);
        }
        if (CONFIG_TRACE_DEPTH > 8) CHECK(entry(8).kind == ZEAL_TRACE_RESET);
        const zeal_trace_entry_t call = entry(0);
        CHECK(call.bytes[0] == 0xCD && call.bytes[1] == 0x00 && call.bytes[2] == 0x03);
        CHECK(call.sp == 0xFFFE);           /* registers before execution */
        CHECK((call.af >> 8) == 0x23);
    }

    /* Maskable interrupt (IM1) accepted after the nop at $0300 */
    z80_gen_int(&s_machine.cpu, 0xFF);
    step();
    CHECK(s_machine.cpu.pc == 0x0038);
    CHECK(stack(f) == 3);
    check_frame(&f[2], ZEAL_FRAME_INT, 0x0301, 0x0301, 0x0038, 0xFFFA);
    CHECK(entry(0).kind == ZEAL_TRACE_IM1);
    CHECK(entry(0).pc == 0x0301);
    CHECK(entry(0).target == 0x0038);
    if (CONFIG_TRACE_DEPTH > 1) CHECK(entry(1).kind == ZEAL_TRACE_INSTR && entry(1).pc == 0x0300);
    step(); /* ei */
    step(); /* reti */
    CHECK(s_machine.cpu.pc == 0x0301);
    CHECK(stack(f) == 2);

    /* NMI */
    z80_gen_nmi(&s_machine.cpu);
    step(); /* nop at $0301, then the NMI */
    CHECK(s_machine.cpu.pc == 0x0066);
    CHECK(stack(f) == 3);
    check_frame(&f[2], ZEAL_FRAME_NMI, 0x0302, 0x0302, 0x0066, 0xFFFA);
    CHECK(entry(0).kind == ZEAL_TRACE_NMI);
    step(); /* retn */
    CHECK(s_machine.cpu.pc == 0x0302);
    CHECK(stack(f) == 2);

    /* Stack switch: the frames that led there stay */
    step(); /* ld ($E000),sp */
    step(); /* ld sp,$F000 */
    CHECK(stack(f) == 2);
    step(); /* call $0400 */
    CHECK(s_machine.cpu.pc == 0x0400);
    CHECK(stack(f) == 3);
    check_frame(&f[2], ZEAL_FRAME_CALL, 0x0309, 0x030C, 0x0400, 0xEFFE);
    CHECK(f[2].segment == f[1].segment + 1);
    step(); /* nop */
    step(); /* ret */
    CHECK(s_machine.cpu.pc == 0x030C);
    CHECK(stack(f) == 2);
    step(); /* ld sp,($E000): back to the first stack */
    CHECK(s_machine.cpu.sp == 0xFFFC);
    CHECK(stack(f) == 2);

    /* Returns, then RST */
    step(); /* ret to $0203 */
    CHECK(s_machine.cpu.pc == 0x0203);
    CHECK(stack(f) == 1);
    step(); /* ret to $010D */
    CHECK(s_machine.cpu.pc == 0x010D);
    CHECK(stack(f) == 0);
    step(); /* rst $10 */
    CHECK(s_machine.cpu.pc == 0x0010);
    CHECK(stack(f) == 1);
    check_frame(&f[0], ZEAL_FRAME_RST, 0x010D, 0x010E, 0x0010, 0xFFFE);
    step(); /* ret */
    CHECK(stack(f) == 0);
    CHECK(zeal_callstack_truncated(&s_machine) == false);

    /* Wrap: the jr $ loop fills the ring several times over */
    run_to(0x010F);
    for (unsigned i = 0; i < 3 * CONFIG_TRACE_DEPTH + 7; i++) step();
    CHECK(zeal_trace_count(&s_machine) == CONFIG_TRACE_DEPTH);
    for (unsigned i = 0; i < CONFIG_TRACE_DEPTH; i++) {
        const zeal_trace_entry_t e = entry(i);
        CHECK(e.pc == 0x010F && e.kind == ZEAL_TRACE_INSTR);
        if (i > 0 && e.cycle + 12 != entry(i - 1).cycle) {
            CHECK(e.cycle + 12 == entry(i - 1).cycle);
            break;
        }
    }
    zeal_trace_entry_t dummy;
    CHECK(!zeal_trace_get(&s_machine, CONFIG_TRACE_DEPTH, &dummy));

    /* Nothing is recorded while the debugger is off */
    const uint64_t serial = zeal_trace_serial(&s_machine);
    for (int i = 0; i < 10; i++) zeal_step(&s_machine, false);
    CHECK(zeal_trace_serial(&s_machine) == serial);
    /* ... and resuming says so */
    step();
    CHECK(zeal_trace_serial(&s_machine) == serial + 2);
    if (CONFIG_TRACE_DEPTH > 1) CHECK(entry(1).kind == ZEAL_TRACE_GAP);
    CHECK(entry(0).kind == ZEAL_TRACE_INSTR);
    CHECK(zeal_callstack_truncated(&s_machine));

    /* Runtime switch */
    zeal_trace_set_enabled(&s_machine, false);
    const uint64_t serial2 = zeal_trace_serial(&s_machine);
    step();
    CHECK(zeal_trace_serial(&s_machine) == serial2);
    zeal_trace_set_enabled(&s_machine, true);

    /* Clear, then reset */
    zeal_trace_clear(&s_machine);
    CHECK(zeal_trace_count(&s_machine) == 0);
    zeal_reset(&s_machine);
    CHECK(zeal_trace_count(&s_machine) == 1);
    CHECK(entry(0).kind == ZEAL_TRACE_RESET);
    CHECK(stack(f) == 0);

    /* Redundant index prefixes still advance the pushed return PC. */
    const uint8_t prefixed_call[] = { 0xDD, 0xFD, 0xDD, 0xCD, 0x00, 0x05 };
    const uint8_t prefixed_ret[] = { 0xFD, 0xC9 };
    poke(0x400, prefixed_call, sizeof(prefixed_call));
    poke(0x500, prefixed_ret, sizeof(prefixed_ret));
    s_machine.cpu.pc = 0x400;
    step();
    CHECK(stack(f) == 1);
    check_frame(&f[0], ZEAL_FRAME_CALL, 0x400, 0x406, 0x500, s_machine.cpu.sp);
    CHECK(entry(0).skipped_prefixes == 3 && entry(0).bytes[0] == 0xCD);
    step();
    CHECK(stack(f) == 0 && s_machine.cpu.pc == 0x406);

    /* The last effective index prefix stays with indexed instructions,
     * including the CB displacement. A discarded prefix on a conditional
     * call/return must not create a frame when the condition is false. */
    const uint8_t indexed[] = {
        0xDD, 0xFD, 0x21, 0x00, 0xD0,  /* LD IY,$D000 */
        0xDD, 0xFD, 0xCB, 0x01, 0x46,  /* BIT 0,(IY+1) */
        0xFD, 0xDD, 0x31, 0x00, 0xEF,  /* LD SP,$EF00 */
        0xDD, 0xF9,                    /* LD SP,IX */
        0xFD, 0xC4, 0x00, 0x05,        /* CALL NZ,$0500 */
    };
    poke(0x600, indexed, sizeof(indexed));
    s_machine.cpu.pc = 0x600;
    step();
    CHECK(s_machine.cpu.iy == 0xD000 && entry(0).skipped_prefixes == 1 && entry(0).bytes[0] == 0xFD);
    step();
    CHECK(entry(0).skipped_prefixes == 1 && entry(0).bytes[1] == 0xCB && entry(0).bytes[3] == 0x46);
    step();
    CHECK(s_machine.cpu.sp == 0xEF00 && entry(0).skipped_prefixes == 2);
    s_machine.cpu.ix = 0xEFFE;
    step();
    CHECK(s_machine.cpu.sp == 0xEFFE && entry(0).skipped_prefixes == 0);
    const uint16_t call_pc = s_machine.cpu.pc;
    s_machine.cpu.zf = 1;
    step();
    CHECK(s_machine.cpu.pc == call_pc + 4 && stack(f) == 0);
    s_machine.cpu.pc = call_pc;
    s_machine.cpu.zf = 0;
    step();
    CHECK(stack(f) == 1 && f[0].ret == call_pc + 4);
    const uint8_t ret_z[] = { 0xFD, 0xC8 }; /* RET Z */
    poke(0x500, ret_z, sizeof(ret_z));
    step();
    CHECK(stack(f) == 1 && s_machine.cpu.pc == 0x502);
    s_machine.cpu.pc = 0x500;
    s_machine.cpu.zf = 1;
    step();
    CHECK(stack(f) == 0 && s_machine.cpu.pc == call_pc + 4);

    mmu_io_write_byte(&s_machine.cpu.mmu, 0xf3, 0x23);
    s_machine.cpu.pc = 0xC000;
    mmu_virt_write_byte(&s_machine.cpu.mmu, 0xC000, 0);
    z80_gen_nmi(&s_machine.cpu);
    step();
    CHECK(entry(0).kind == ZEAL_TRACE_NMI && entry(0).pc == 0xC001);
    CHECK(entry(0).target == 0x0066 && entry(0).page == 0x23);

    zeal_deinit(&s_machine);
    if (s_failures == 0) printf("trace: all checks passed (depth %u)\n", zeal_trace_capacity());
    return s_failures ? 1 : 0;
}

static double run_bench(const char* rom, unsigned long tstates, int mode)
{
    if (init_machine(true)) exit(1);
    FILE* f = fopen(rom, "rb");
    if (f == NULL) {
        perror(rom);
        exit(1);
    }
    const size_t n = fread(s_machine.rom.data, 1, s_machine.rom.size, f);
    fclose(f);
    if (n == 0) exit(1);
    s_machine.dbg_enabled = mode != 0;
    zeal_trace_set_enabled(&s_machine, mode == 2);
    zeal_reset(&s_machine);
    s_machine.dbg_state = ST_RUNNING;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    const uint64_t start = s_machine.cpu.cyc;
    while (s_machine.cpu.cyc - start < tstates) {
        if (zeal_step(&s_machine, mode != 0) & (ZEAL_STEP_STOP | ZEAL_STEP_FATAL)) break;
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    zeal_deinit(&s_machine);
    return (double) (b.tv_sec - a.tv_sec) + (double) (b.tv_nsec - a.tv_nsec) / 1e9;
}

int main(int argc, char** argv)
{
    if (argc >= 3 && strcmp(argv[1], "--bench") == 0) {
        const unsigned long mt = argc >= 4 ? strtoul(argv[3], NULL, 0) : 500;
        static const char* names[] = { "debugger off", "debugger on, trace off", "debugger on, trace on" };
        double best[3] = { 1e9, 1e9, 1e9 };
        for (int round = 0; round < 3; round++) {
            for (int mode = 0; mode < 3; mode++) {
                const double t = run_bench(argv[2], mt * 1000000UL, mode);
                if (t < best[mode]) best[mode] = t;
            }
        }
        for (int mode = 0; mode < 3; mode++) {
            printf("%-24s %7.3f s  %6.1f MT/s  %+5.1f%%\n", names[mode], best[mode],
                   (double) mt / best[mode], 100.0 * (best[mode] / best[0] - 1.0));
        }
        return 0;
    }
    return run_checks();
}
