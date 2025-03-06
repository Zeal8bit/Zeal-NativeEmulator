#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hw/zeal.h"
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
        printf("TODO: MEM PHYSICAL ADDRESS READ: %08x\n", addr);
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
        printf("TODO: PHYSICAL ADDRESS WRITE\n");
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


static void zeal_debugger_step_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_REQ_STEP;
}


static void zeal_debugger_step_over_cb(dbg_t* dbg) {
    zeal_t* machine = (zeal_t*) (dbg->arg);
    machine->dbg_state = ST_REQ_STEP_OVER;
}


/**
 * ===========================================================
 *                  DISASSEMBLER RELATED
 * ===========================================================
 */

extern const instr_data_t disassembled_z80_opcodes[256];

int zeal_debugger_disassemble_address(dbg_t *dbg, hwaddr address, dbg_instr_t* instr)
{
    const char* label = NULL;
    uint8_t mem[4];

    if (instr == NULL) {
        return -1;
    }

    /* Instructions are never bigger than 4 bytes on the Z80 */
    if (zeal_debugger_get_mem(dbg, address, 4, instr->opcodes) != 0) {
        printf("Error disassembling address: %x\n", address);
        return -1;
    }
    memcpy(mem, instr->opcodes, 4);

    /* Get the opcode */
    const size_t max_len = sizeof(instr->instruction);
    const instr_data_t* opcode = disassembled_z80_opcodes + mem[0];

    if (opcode->fmt == NULL) {
        /* Not implemented, return 4 */
        return 4;
    }

    if (opcode->size == 1) {
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
    } else if (opcode->size == 3) {
        /* Check if the instruction could have a label */
        const uint16_t dest = MAKE16(mem[2], mem[1]);
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
    dbg->step_cb = zeal_debugger_step_cb;
    dbg->step_over_cb = zeal_debugger_step_over_cb;
    dbg->get_regs_cb = zeal_debugger_get_regs;
    dbg->set_regs_cb = zeal_debugger_set_regs;
    dbg->get_mem_cb = zeal_debugger_get_mem;
    dbg->set_mem_cb = zeal_debugger_set_mem;
    dbg->disassemble_cb = zeal_debugger_disassemble_address;

    return 0;
}
