#pragma once

#include <stdint.h>
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


/**
 * @brief Type to represent a breakpoint
*/
typedef struct {
    uint8_t     type;
    uint16_t    addr;
} zeal_breakpoint_t;


/**
 * @brief Alias for the target GDB operations
 */
typedef struct target_ops target_ops;


/**
 * @brief Enumaeration of all the registers pair, as expected by GDB
 */
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


/**
 * @brief Initialize the debug-related structure in the given Zeal machine
 */
int zeal_dbg_init(void* machine);


/**
 * @brief Run the debug server with the given Zeal machine
 */
int zeal_run_dbg(void* machine);
