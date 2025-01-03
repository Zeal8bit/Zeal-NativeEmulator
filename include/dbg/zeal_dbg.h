#pragma once

#include <stdint.h>
#include "hw/zeal.h"
#include "dbg/gdbstub.h"
#define DEBUG_PORT  "5555"

/**
 * @brief GDB interprets all Z80 registers as 16-bit ones (pairs only)
 */
#define ZEAL_Z80_REG_BYTES  2


/**
 * @brief The Z80 CPU has 13 pairs
 */
#define ZEAL_Z80_NUM_REGS   13


typedef struct target_ops target_ops;

typedef enum
{
    Z80_AF_REG  = 0,
    Z80_BC_REG  = 1,
    Z80_DE_REG  = 2,
    Z80_HL_REG  = 3,
    Z80_SP_REG  = 4,
    Z80_PC_REG  = 5,
    Z80_IX_REG  = 6,
    Z80_IY_REG  = 7,
    Z80_AFP_REG = 8, // Prime registers
    Z80_BCP_REG = 9, // Prime registers
    Z80_DEP_REG = 10, // Prime registers
    Z80_HLP_REG = 11, // Prime registers
    Z80_IR_REG  = 12,
    Z80_REG_LAST = Z80_IR_REG
} zeal_z80_reg_t;


static int zeal_dbg_init(void* machine);

/**
 * @brief Callback invoked when a register needs to be read
 */
static int zeal_dbg_read_reg(void *args, int regno, size_t *reg_value);

/**
 * @brief Callback invoked when a register needs to be written
 */
static int zeal_dbg_write_reg(void *args, int regno, size_t data);

/**
 * @brief Callback invoked when a byte in memory needs to be read
 */
static int zeal_dbg_read_mem(void *args, size_t addr, size_t len, void *val);

/**
 * @brief Callback invoked when a byte in memory needs to be modified
 */
static int zeal_dbg_write_mem(void *args, size_t addr, size_t len, void *val);

/**
 * @brief Callback invoked when the debugger wants to continue execution
 */
static gdb_action_t zeal_dbg_cont(void *args);

/**
 * @brief Callback invoked when the debugger wants to single step an instruction
 */
static gdb_action_t zeal_dbg_stepi(void *args);

/**
 * @brief Callback invoked when the debugger adds a breakpoint
 */
static bool zeal_dbg_set_bp(void *args, size_t addr, bp_type_t type);

/**
 * @brief Callback invoked when the debugger deletes a breakpoint
 */
static bool zeal_dbg_del_bp(void *args, size_t addr, bp_type_t type);

/**
 * @brief Callback invoked when the debugger interrupts the current CPU execution
 */
static void zeal_dbg_on_interrupt(void *args);

