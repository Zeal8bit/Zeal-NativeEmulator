#pragma once

#include <stdint.h>

/* Debugger context, need to be defined in an implementation */
typedef struct dbg_t dbg_t;
typedef struct regs_t regs_t;

/* Custom type for the hardware address */
typedef uint32_t hwaddr;


/* Debugger event types */
typedef enum {
    DEBUG_EVENT_NONE,
    DEBUG_EVENT_BREAKPOINT_HIT,
    DEBUG_EVENT_STEP_COMPLETE,
    DEBUG_EVENT_MEMORY_WATCH,
    DEBUG_EVENT_CPU_EXCEPTION
} debug_event_t;



/**
 * @brief States for the machine execution
 */
typedef enum {
    ST_RUNNING,
    ST_PAUSED,
    ST_REQ_STEP,
    ST_REQ_STEP_OVER,
} dbg_state_t;


/* Watchpoint types */
typedef enum {
    WATCHPOINT_READ  = 1,
    WATCHPOINT_WRITE = 2,
    WATCHPOINT_RW    = WATCHPOINT_WRITE | WATCHPOINT_READ
} watchpoint_type_t;


/**
 * @brief Type for disassembled instructions
 */
typedef struct {
    char label[64];
    char instruction[64];
    /* Size of the instruction in bytes */
    int size;
    /* Bytes composing the instruction */
    uint8_t opcodes[4];
} dbg_instr_t;


typedef struct {
    char*       fmt;
    char*       fmt_lab;
    uint32_t    size : 3;
    uint32_t    label : 1;
} instr_data_t;
