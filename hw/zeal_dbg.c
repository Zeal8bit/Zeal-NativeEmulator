#include <stdio.h>
#include "hw/zeal.h"
#include "dbg/gdbstub.h"
#include "dbg/zeal_dbg.h"

#if CONFIG_ENABLE_GDB_SERVER

#define MAKE16(a,b)     ((a) << 8 | (b))
#define MSB8(a)         (((a) >> 8) & 0xff)
#define LSB8(a)         (((a) >> 0) & 0xff)


/**
 * @brief Callback invoked when a register needs to be read
 */
static int zeal_dbg_read_reg(void *args, int regno, size_t *reg_value)
{
    zeal_t* machine = (zeal_t*) args;
    if (regno > Z80_REG_LAST) {
        return -1;
    }

    z80* cpu = &machine->cpu;

    switch(regno) {
        case Z80_AF_REG:
            *reg_value = MAKE16(cpu->a, z80_get_f(cpu));
            break;
        case Z80_BC_REG:
            *reg_value = MAKE16(cpu->b, cpu->c);
            break;
        case Z80_DE_REG:
            *reg_value = MAKE16(cpu->d, cpu->e);
            break;
        case Z80_HL_REG:
            *reg_value = MAKE16(cpu->h, cpu->l);
            break;
        case Z80_SP_REG:
            *reg_value = cpu->sp;
            break;
        case Z80_PC_REG:
            *reg_value = cpu->pc;
            break;
        case Z80_IX_REG:
            *reg_value = cpu->ix;
            break;
        case Z80_IY_REG:
            *reg_value = cpu->iy;
            break;
        case Z80_AFP_REG:
            *reg_value = MAKE16(cpu->a_, cpu->f_);
            break;
        case Z80_BCP_REG:
            *reg_value = MAKE16(cpu->b_, cpu->c_);
            break;
        case Z80_DEP_REG:
            *reg_value = MAKE16(cpu->d_, cpu->e_);
            break;
        case Z80_HLP_REG:
            *reg_value = MAKE16(cpu->h_, cpu->l_);
            break;
        case Z80_IR_REG:
            *reg_value = MAKE16(cpu->i, cpu->r);
            break;
    }

    return 0;
}


/**
 * @brief Callback invoked when a register needs to be written
 */
static int zeal_dbg_write_reg(void *args, int regno, size_t data)
{
    zeal_t* machine = (zeal_t*) args;
    if (regno > Z80_REG_LAST) {
        return -1;
    }

    z80* cpu = &machine->cpu;
    switch(regno) {
        case Z80_AF_REG:
            cpu->a = MSB8(data);
            z80_set_f(cpu, LSB8(data));
            break;
        case Z80_BC_REG:
            cpu->b = MSB8(data);
            cpu->c = LSB8(data);
            break;
        case Z80_DE_REG:
            cpu->d = MSB8(data);
            cpu->e = LSB8(data);
            break;
        case Z80_HL_REG:
            cpu->h = MSB8(data);
            cpu->l = LSB8(data);
            break;
        case Z80_SP_REG:
            cpu->sp = data & 0xffff;
            break;
        case Z80_PC_REG:
            cpu->pc = data & 0xffff;
            break;
        case Z80_IX_REG:
            cpu->ix = data & 0xffff;
            break;
        case Z80_IY_REG:
            cpu->iy = data & 0xffff;
            break;
        case Z80_AFP_REG:
            cpu->a_ = MSB8(data);
            cpu->f_ = LSB8(data);
            break;
        case Z80_BCP_REG:
            cpu->b_ = MSB8(data);
            cpu->c_ = LSB8(data);
            break;
        case Z80_DEP_REG:
            cpu->d_ = MSB8(data);
            cpu->e_ = LSB8(data);
            break;
        case Z80_HLP_REG:
            cpu->h_ = MSB8(data);
            cpu->l_ = LSB8(data);
            break;
        case Z80_IR_REG:
            cpu->i = MSB8(data);
            cpu->r = LSB8(data);
            break;
    }
    return 0;
}


/**
 * @brief Callback invoked when a byte in memory needs to be read
 */
static int zeal_dbg_read_mem(void *args, size_t addr, size_t len, void *val)
{
    printf("%s, 0x%lx, 0x%lx\n", __func__, addr, len);
    uint8_t* val_ptr = (uint8_t*) val;
    zeal_t* machine = (zeal_t*) args;

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        printf("TODO: PHYSICAL ADDRESS READ\n");
    } else if (addr <= 0xffff) {
        for (size_t i = 0; i < len; i++) {
            // TODO: Use a specialized function from Zeal component?
            val_ptr[i] = machine->cpu.read_byte(args, (uint16_t) (addr + i));
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


/**
 * @brief Callback invoked when a byte in memory needs to be modified
 */
static int zeal_dbg_write_mem(void *args, size_t addr, size_t len, void *val)
{
    printf("%s\n", __func__);
    uint8_t* val_ptr = (uint8_t*) val;
    zeal_t* machine = (zeal_t*) args;

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        printf("TODO: PHYSICAL ADDRESS READ\n");
    } else if (addr <= 0xffff) {
        for (size_t i = 0; i < len; i++) {
            // TODO: Use a specialized function from Zeal component?
            machine->cpu.write_byte(args, (uint16_t) (addr + i), val_ptr[i]);
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


static bool zeal_dbg_pc_in_breakpoints(zeal_t* machine)
{
    const uint16_t addr = machine->cpu.pc;
    // FIXME: Naive implementation... Using a binary tree would be better
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == addr) {
            return true;
        }
    }
    return false;
}


/**
 * @brief Callback invoked when the debugger wants to continue execution
 */
static gdb_action_t zeal_dbg_cont(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;

    do {
        // FIXME: Use the (future) "generic" step function which takes into
        // account the t-states callbacks and the GUI timings.
        z80_step(&machine->cpu);
    } while (!machine->brk_exec && !zeal_dbg_pc_in_breakpoints(machine));

    printf("Stopped at 0x%x\n", machine->cpu.pc);

    return ACT_RESUME;
}


/**
 * @brief Callback invoked when the debugger wants to single step an instruction
 */
static gdb_action_t zeal_dbg_stepi(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;
    z80_step(&machine->cpu);
    return ACT_RESUME;
}


/**
 * @brief Callback invoked when the debugger adds a breakpoint
 */
static bool zeal_dbg_set_bp(void *args, size_t addr, bp_type_t type)
{
    printf("%s 0x%lx (%d)\n", __func__, addr, type);
    zeal_t* machine = (zeal_t*) args;

    if (addr > 0xffff) {
        return false;
    }

    // FIXME: Very naive implementation currently
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == 0) {
            machine->breakpoints[i].addr = addr;
            return true;
        }
    }

    (void) type;
    return false;
}


/**
 * @brief Callback invoked when the debugger deletes a breakpoint
 */
static bool zeal_dbg_del_bp(void *args, size_t addr, bp_type_t type)
{
    printf("%s 0x%lx (%d)\n", __func__, addr, type);
    zeal_t* machine = (zeal_t*) args;

    if (addr > 0xffff) {
        return true;
    }

    // FIXME: Very naive implementation currently
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == addr) {
            machine->breakpoints[i].addr = 0;
            return true;
        }
    }

    (void) type;
    return true;
}


/**
 * @brief Callback invoked when the debugger interrupts the current CPU execution
 */
static void zeal_dbg_on_interrupt(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;
    machine->brk_exec = true;
}


/**
 * @brief Initialize the debug-related structure in the given Zeal machine
 */
int zeal_dbg_init(void* machine)
{
    zeal_t *m = (zeal_t *) machine;

    /* We need to provide the number of registers (16-bit pairs) and the size of each */
    m->dbg_arch = (arch_info_t) {
        .reg_num     = ZEAL_Z80_NUM_REGS,
        .reg_byte    = ZEAL_Z80_REG_BYTES,
        .target_desc = "z80-full",
    };
    
    /* Populate all the callbacks used for debugging */
    m->dbg_ops = (target_ops) {
        .read_reg = zeal_dbg_read_reg,
        .write_reg = zeal_dbg_write_reg,
        .read_mem = zeal_dbg_read_mem,
        .write_mem = zeal_dbg_write_mem,
        .cont = zeal_dbg_cont,
        .stepi = zeal_dbg_stepi,
        .set_bp = zeal_dbg_set_bp,
        .del_bp = zeal_dbg_del_bp,
        .on_interrupt = zeal_dbg_on_interrupt,
    };

    return 0;
}


int zeal_run_dbg(void* machine)
{
    zeal_t *m = (zeal_t *) machine;
    printf("Waiting for GDB client to connect on port " DEBUG_PORT "\n");
    bool success = gdbstub_init(&m->dbg_serv, &m->dbg_ops,
                                m->dbg_arch, "127.0.0.1:" DEBUG_PORT);
    if (!success) {
        printf("%s: error initializing GDB server\n", __func__);
        return 1;
    }

    /* Try to start the server */
    printf("Connected!\n");
    if (!gdbstub_run(&m->dbg_serv, m)) {
        fprintf(stderr, "%s: fail to run in debug mode.\n", __func__);
        return 2;
    }

    return 0;
}

#endif // CONFIG_ENABLE_GDB_SERVER
