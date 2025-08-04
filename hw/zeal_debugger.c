/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hw/zeal.h"
#include "utils/log.h"
#include "debugger/debugger_impl.h"


#define MAKE16(a,b)     ((a) << 8 | (b))
#define MSB8(a)         (((a) >> 8) & 0xff)
#define LSB8(a)         (((a) >> 0) & 0xff)


/**
 * @brief Callback invoked when registers need to be read
 */
static void zeal_debugger_get_regs(dbg_t *dbg, regs_t *regs)
{
    zeal_t* machine = (zeal_t*) (dbg->arg);
    z80* cpu = &machine->cpu;

    *regs = (regs_t) {
        .pc  = cpu->pc,
        .sp  = cpu->sp,
        .a   = cpu->a, .f   = z80_get_f(cpu),
        .b   = cpu->b, .c   = cpu->c,
        .d   = cpu->d, .e   = cpu->e,
        .h   = cpu->h, .l   = cpu->l,
        .a_  = cpu->a_, .f_ = cpu->f_,
        .b_  = cpu->b_, .c_ = cpu->c_,
        .d_  = cpu->d_, .e_ = cpu->e_,
        .h_  = cpu->h_, .l_ = cpu->l_,
        .i   = cpu->i, .r   = cpu->r,
        .ix  = cpu->ix,
        .iy  = cpu->iy,
    };
}


static void zeal_debugger_set_regs(dbg_t *dbg, regs_t *regs)
{
    zeal_t* machine = (zeal_t*) (dbg->arg);
    z80* cpu = &machine->cpu;

    cpu->sp = regs->sp;
    cpu->pc = regs->pc;
    cpu->a  = regs->a;  z80_set_f(cpu, regs->f);
    cpu->b  = regs->b;  cpu->c  = regs->c;
    cpu->d  = regs->d;  cpu->e  = regs->e;
    cpu->h  = regs->h;  cpu->l  = regs->l;
    cpu->a_ = regs->a;  cpu->f_ = regs->f_;
    cpu->b_ = regs->b_; cpu->c_ = regs->c_;
    cpu->d_ = regs->d_; cpu->e_ = regs->e_;
    cpu->h_ = regs->h_; cpu->l_ = regs->l_;
    cpu->i  = regs->i;  cpu->r  = regs->r;
    cpu->ix = regs->ix;
    cpu->iy = regs->iy;
}



/**
 * @brief Callback invoked memory needs to be read
 */
static int zeal_debugger_get_mem(dbg_t *dbg, hwaddr addr, int len, uint8_t *val)
{
    zeal_t* machine = (zeal_t*) (dbg->arg);

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        log_printf("TODO: MEM PHYSICAL ADDRESS READ: %08x\n", addr);
    } else if (addr <= 0xffff) {
        for (int i = 0; i < len; i++) {
            // TODO: Use a specialized function from Zeal component?
            val[i] = machine->cpu.read_byte(machine, (uint16_t) (addr + i));
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


/**
 * @brief Callback invoked when memory needs to be modified
 */
static int zeal_debugger_set_mem(dbg_t *dbg, hwaddr addr, int len, uint8_t *val)
{
    zeal_t* machine = (zeal_t*) (dbg->arg);

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        log_printf("TODO: PHYSICAL ADDRESS WRITE\n");
    } else if (addr <= 0xffff) {
        for (int i = 0; i < len; i++) {
            machine->cpu.write_byte(machine, (uint16_t) (addr + i), val[i]);
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


static void zeal_debugger_pause_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_PAUSED;
}

static bool zeal_debugger_is_paused_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    /* To prevent the view from blinking (because of pause/continue/pause/...), consider that
     * a step request is also a pause. */
    return machine->dbg_state == ST_PAUSED || machine->dbg_state == ST_REQ_STEP;
}


static void zeal_debugger_continue_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_RUNNING;
}

static void zeal_debugger_reset_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    log_printf("[SYSTEM] Reset\n");
    zeal_reset(machine);
}


static void zeal_debugger_step_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_REQ_STEP;
}


static void zeal_debugger_step_over_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_REQ_STEP_OVER;
}

static void zeal_debugger_breakpoint_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);

    if(machine->dbg_state != ST_RUNNING) {
        debugger_toggle_breakpoint(dbg, machine->cpu.pc);
    }
}


/**
 * ===========================================================
 *                  DISASSEMBLER RELATED
 * ===========================================================
 */

extern const instr_data_t disassembled_z80_opcodes[256];
extern const instr_data_t disassembled_cb_opcodes[256];
extern const instr_data_t disassembled_ed_opcodes[256];
extern const instr_data_t disassembled_ixiy_opcodes[256];


static const instr_data_t* process_ed_instr(uint8_t snd)
{
    static char fmt[2][64];
    static instr_data_t opcode = {
        .fmt = fmt[0],
        .fmt_lab = fmt[1],
        .size = 4,
        .label = true
    };
    const char* regs_16[] = { "bc", "de", "hl", "sp"};
    const int reg_idx = (snd >> 4) & 3;
    const char* pair = regs_16[reg_idx];


    if ((snd & 0x4B) == 0x4B) {
        /* Special case for LD dd, (nn) */
        sprintf(opcode.fmt, "ld     %s, (0x%%04x)", pair);
        sprintf(opcode.fmt_lab, "ld     %s, (%%s)", pair);
    } else if ((snd & 0x43) == 0x43) {
        /* Special case for LD (nn), dd */
        sprintf(opcode.fmt, "ld     (0x%%04x), %s", pair);
        sprintf(opcode.fmt_lab, "ld     (%%s), %s", pair);
    }

    return &opcode;
}


static const char* processes_ixiy_cb(uint8_t fourth)
{
    switch (fourth){
        case 0x06: return "rlc (i%c%+d)";
        case 0x0e: return "rrc (i%c%+d)";
        case 0x16: return "rl  (i%c%+d)";
        case 0x1e: return "rr  (i%c%+d)";
        case 0x26: return "sla (i%c%+d)";
        case 0x2e: return "sra (i%c%+d)";
        case 0x3e: return "srl (i%c%+d)";
        default: {
            static char fmt[] = "bit 0,(i%c%+d)";
            /* Set the bit value */
            const char bit = (fourth >> 3) & 0x7;
            fmt[4] = bit + '0';
            if ((fourth & 0xc7) == 0x46) {
                return fmt;
            } else if ((fourth & 0xc7) == 0x86) {
                fmt[0] = 'r';
                fmt[1] = 'e';
                fmt[2] = 's';
                return fmt;
            } else if ((fourth & 0xc7) == 0xc6) {
                fmt[0] = 's';
                fmt[1] = 'e';
                fmt[2] = 't';
                return fmt;
            }
            return "ill";
        }
    }
}


static int process_ixiy_opcodes(const uint8_t mem[4], dbg_instr_t* instr)
{
    /* Choose between IX and IY register */
    const char reg_letter = (mem[0] == 0xdd) ? 'x' : 'y';
    const instr_data_t* opcode = disassembled_ixiy_opcodes + mem[1];

    /* special case for 0xCB and 16-bit LD instructions */
    if (opcode->fmt == NULL) {
        if (mem[1] == 0xCB) {
            const char* fmt = processes_ixiy_cb(mem[3]);
            snprintf(instr->instruction, sizeof(instr->instruction), fmt, reg_letter, mem[2]);
        } else if ((mem[1] & 0xf8) == 0x70) {
            /* Special case because the opcode contains the parameter */
            const char  reg_idx[] = { 'b', 'c', 'd', 'e', 'h', 'l', '?', 'a' };
            const char* fmt = "ld     (i%c%+d),%c";
            snprintf(instr->instruction, sizeof(instr->instruction), fmt, reg_letter, mem[2], (reg_idx[mem[1] & 0x7]));
            return 3;
        }

        return 4;
    }

    /* If the instruction has a size of 2, we only need to provide the register letter */
    if (opcode->size == 2) {
        /* Some instruction may need the same register twice, pass it twice */
        snprintf(instr->instruction, sizeof(instr->instruction), opcode->fmt, reg_letter, reg_letter);
    } else if (opcode->size == 3) {
        snprintf(instr->instruction, sizeof(instr->instruction), opcode->fmt, reg_letter, mem[2]);
    } else if (opcode->size == 4) {
        /* Special case for `ld (ix+d), n` and `ld (nnnn), ix` */
        if (mem[1] == 0x36) {
            snprintf(instr->instruction, sizeof(instr->instruction), opcode->fmt, reg_letter, mem[2], mem[3]);
        } else if (mem[1] == 0x22) {
            snprintf(instr->instruction, sizeof(instr->instruction), opcode->fmt, mem[3], mem[2], reg_letter);
        } else {
            snprintf(instr->instruction, sizeof(instr->instruction), opcode->fmt, reg_letter, mem[3], mem[2]);
        }
    }

    return opcode->size;
}


int zeal_debugger_disassemble_address(dbg_t *dbg, hwaddr address, dbg_instr_t* instr)
{
    const char* label = NULL;
    uint8_t mem[4];

    if (instr == NULL) {
        return -1;
    }

    /* Instructions are never bigger than 4 bytes on the Z80 */
    if (zeal_debugger_get_mem(dbg, address, 4, instr->opcodes) != 0) {
        log_err_printf("[DEBUGGER] Error disassembling address: %x\n", address);
        return -1;
    }
    memcpy(mem, instr->opcodes, 4);

    /* By default, clear the returned instruction */
    instr->instruction[0] = 0;

    /* Get the opcode */
    bool no_param_instr = false;
    const bool is_cb = (mem[0] == 0xcb);
    const bool is_ed = (mem[0] == 0xed);
    const bool is_fd = (mem[0] == 0xfd);
    const bool is_dd = (mem[0] == 0xdd);
    const size_t max_len = sizeof(instr->instruction);
    const instr_data_t* opcode;

    if (is_ed) {
        opcode = disassembled_ed_opcodes + mem[1];
        /* Check if the array entry is populated */
        if (opcode->fmt == NULL) {
            opcode = process_ed_instr(mem[1]);
        } else {
            no_param_instr = true;
        }
    } else if (is_cb) {
        opcode = disassembled_cb_opcodes + mem[1];
        /* CB instructions must not be treated like other 2-byte instructions */
        no_param_instr = true;
    } else if (is_fd || is_dd) {
        return process_ixiy_opcodes(mem, instr);
    } else {
        opcode = disassembled_z80_opcodes + mem[0];
    }

    if (opcode->fmt == NULL) {
        /* Not implemented, return 4 */
        return 4;
    }

    if (opcode->size == 1 || no_param_instr) {
        strncpy(instr->instruction, opcode->fmt, max_len - 1);
        instr->instruction[max_len - 1] = 0;
    } else if (opcode->size == 2) {
        /* If there is a label, here, the parameter is a displacement, check if there is a destination */
        const int8_t displacement = mem[1] + 2;
        if (opcode->label && (label = debugger_get_symbol(dbg, address + displacement))) {
            snprintf(instr->instruction, max_len, opcode->fmt_lab, label);
        } else {
            snprintf(instr->instruction, max_len, opcode->fmt, mem[1]);
        }
    } else {
        /* Check if the instruction could have a label */
        uint16_t dest = 0;
        if (opcode->size == 3) {
            dest = MAKE16(mem[2], mem[1]);
        } else {
            /* Size == 4 */
            dest = MAKE16(mem[3], mem[2]);
        }
        if (opcode->label && (label = debugger_get_symbol(dbg, dest))) {
            snprintf(instr->instruction, max_len, opcode->fmt_lab, label);
        } else {
            /* Either the instruction doesn't have a label, or the label was not found */
            snprintf(instr->instruction, max_len, opcode->fmt, dest);
        }
    }

    return opcode->size;
}


/**
 * @brief Initialize the debug-related structure in the given Zeal machine
 */
int zeal_debugger_init(zeal_t* machine, dbg_t* dbg)
{
    if (machine == NULL || dbg == NULL) {
        return -1;
    }

    dbg->arg = machine;
    dbg->pause_cb = zeal_debugger_pause_cb;
    dbg->is_paused_cb = zeal_debugger_is_paused_cb;
    dbg->continue_cb = zeal_debugger_continue_cb;
    dbg->reset_cb = zeal_debugger_reset_cb;
    dbg->step_cb = zeal_debugger_step_cb;
    dbg->step_over_cb = zeal_debugger_step_over_cb;
    dbg->breakpoint_cb = zeal_debugger_breakpoint_cb;
    dbg->get_regs_cb = zeal_debugger_get_regs;
    dbg->set_regs_cb = zeal_debugger_set_regs;
    dbg->get_mem_cb = zeal_debugger_get_mem;
    dbg->set_mem_cb = zeal_debugger_set_mem;
    dbg->disassemble_cb = zeal_debugger_disassemble_address;

    return 0;
}
