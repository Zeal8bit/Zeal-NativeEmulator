/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "debugger/debugger_types.h"

/* Get MMU info */
#define ZEAL_DBG_OP_GET_MMU     1

/* Number of pages in the MMU info structure */
#define ZEAL_DBG_MMU_PAGES      4

/* MMU Debug types */
typedef struct {
    uint16_t    virt_addr;
    uint8_t     value;
    uint32_t    phys_addr;
    const char* device; // Device being mapped
} dbg_mmu_entry_t;

typedef struct {
    dbg_mmu_entry_t entries[ZEAL_DBG_MMU_PAGES];
} dbg_mmu_t;

/**
 * @brief Disassemble the instruction made of `bytes` as if it were at `address`
 * (relative jumps and labels are resolved from it). Returns its size.
 * Used for instructions recorded by the execution trace.
 */
bool zeal_debugger_uses_index(uint8_t opcode);
int zeal_debugger_disassemble_bytes(dbg_t *dbg, hwaddr address, const uint8_t bytes[4], dbg_instr_t* instr);
