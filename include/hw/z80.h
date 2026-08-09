/**
 * SPDX-FileCopyrightText: 2019-2026 superzazu/Nicolas Allemand <contact@nicolasallemand.com>; 2024 Zeal 8-bit Computer
 * <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Original source code comes from https://github.com/superzazu/z80
 * Modifications made by @zeal8bit: make I/O port operations on 16-bit addresses.
 * Integrate the MMU in the CPU for performance reasons
 */

#ifndef Z80_Z80_H_
#define Z80_Z80_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mmu.h"

typedef struct z80 z80;
struct z80 {
    /* MMU is embedded in the CPU for direct access from the emulation core,
     * eliminating function pointer indirection on every memory access. */
    mmu_t mmu;

    unsigned long cyc; // cycle count (t-states)

    uint16_t pc, sp, ix, iy;                // special purpose registers
    uint16_t mem_ptr;                       // "wz" register
    uint8_t a, b, c, d, e, h, l;            // main registers
    uint8_t a_, b_, c_, d_, e_, h_, l_, f_; // alternate registers
    uint8_t i, r;                           // interrupt vector, memory refresh

    // flags: sign, zero, yf, half-carry, xf, parity/overflow, negative, carry
    bool sf : 1, zf : 1, yf : 1, hf : 1, xf : 1, pf : 1, nf : 1, cf : 1;

    uint8_t iff_delay;
    uint8_t interrupt_mode;
    uint8_t int_data;
    bool iff1 : 1, iff2 : 1;
    bool halted      : 1;
    bool int_pending : 1, nmi_pending : 1;
};

static inline mmu_t* z80_get_mmu(z80* z)
{
    return &z->mmu;
}

void z80_init(z80* const z);
void z80_reset(z80* const z);
int z80_instruction_size(z80* const z);
unsigned long z80_run_for(z80* const z, unsigned long tstates);
int  z80_step(z80* const z);
void z80_debug_output(z80* const z);
void z80_get_debug_output(z80* const z, char* s);
void z80_gen_nmi(z80* const z);
void z80_gen_int(z80* const z, uint8_t data);

/* Helpers to manipulate the flag register */
uint8_t z80_get_f(z80* const z);
void z80_set_f(z80* const z, uint8_t val);

#endif // Z80_Z80_H_
