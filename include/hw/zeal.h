/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hw/z80.h"
#include "hw/device.h"
#include "hw/mmu.h"
#include "hw/flash.h"
#include "hw/ram.h"
#include "hw/zvb/zvb.h"
#include "hw/pio.h"
#include "hw/i2c.h"
#include "hw/keyboard.h"
#include "hw/uart.h"
#include "hw/hostfs.h"
#include "hw/compactflash.h"
#include "hw/semihost.h"
#include "utils/vtimer.h"
#include "hw/userport/snes_adapter.h"

#include "hw/i2c.h"
#include "hw/i2c/ds1307.h"
#include "hw/i2c/at24c512.h"

#if CONFIG_ENABLE_DEBUGGER
#include "debugger/debugger.h"
#endif

struct zeal_t {
    /* The MMU is integrated in the CPU now, it also manages the memory mapping  */
    z80     cpu;
    flash_t rom;
    ram_t   ram;
    zvb_t   zvb;
    pio_t   pio;
    keyboard_t keyboard;
    uart_t uart;
    semihost_t semihost;
    /* I2C related */
    i2c_t    i2c_bus;
    ds1307_t rtc;
    at24c512_t eeprom;
    compactflash_t compactflash;
    snes_adapter_t snes_adapter;

    /* Host callbacks and execution policy */
    const hw_host_t* host;
    bool stop_on_reset;
    void* reset_userdata;
    void (*reset_hook)(void*, struct zeal_t*, bool after_devices);
    bool should_exit;

    /* Misc features */
    zeal_hostfs_t hostfs;


    /* Debugger related */
#if CONFIG_ENABLE_DEBUGGER
    bool             dbg_enabled;
    dbg_state_t      dbg_state;
    dbg_t            dbg;
    uint8_t        (*dbg_read_memory)(struct zeal_t*, hwaddr addr);
#endif
};

typedef struct zeal_t zeal_t;


typedef struct {
    const hw_host_t* host;
    bool frames;
    bool debugger;
    bool stop_on_reset;
    bool no_dma;
    bool no_scanline_rendering;
} zeal_options_t;

typedef enum {
    ZEAL_STEP_NONE = 0,
    ZEAL_STEP_FRAME = 1,
    ZEAL_STEP_STOP = 2,
    ZEAL_STEP_DEBUG_STOP = 4,
    ZEAL_STEP_FATAL = 8,
} zeal_step_result_t;

typedef struct {
    uint64_t elapsed_tstates;
    /* NONE means budget exhausted; otherwise STOP, DEBUG_STOP, or FATAL. */
    zeal_step_result_t reason;
} zeal_run_result_t;

/* Caller owns a zeroable machine and its borrowed host/media callbacks.
 * Calls are synchronous and non-concurrent. Reset is permitted from timer
 * callbacks; elapsed execution is measured by the monotonic virtual clock.
 * reset_hook runs after CPU reset, before devices, then after device reset.
 * Input/timer callbacks may reset or change debugger state; media callbacks
 * may complete pending host requests. Callbacks must not recursively run or
 * destroy the machine. Detach host bindings before deinit; deinit also accepts
 * a partially initialized machine after zeal_init fails. */
/**
 * @brief Initialize a machine and its devices using the supplied host callbacks and options.
 */
int zeal_init(zeal_t*, const zeal_options_t*);
/**
 * @brief Reset the CPU and devices, invoking the frontend reset hook at each boundary.
 */
int                zeal_reset(zeal_t*);
/**
 * @brief Release machine resources after the frontend has detached its host bindings.
 */
void               zeal_deinit(zeal_t*);
/**
 * @brief Advance the machine once and report frame readiness or an execution stop.
 */
zeal_step_result_t zeal_step(zeal_t*, bool debug);
/* Run to a T-state budget (with instruction overshoot), or until stop when
 * budget is zero. Use zeal_step and its FRAME bit to stop at each frame. */
zeal_run_result_t zeal_run(zeal_t*, unsigned long max_tstates);
/**
 * @brief Run the machine for a given number of Z80 T-states (console wait command).
 */
zeal_run_result_t zeal_run_for_tstates(zeal_t*, unsigned long tstates);
/**
 * @brief Request a stop; the frontend is responsible for closing its window and resources.
 */
void              zeal_exit(zeal_t*);
#if CONFIG_ENABLE_DEBUGGER
/**
 * @brief Run while the debugger permits execution; return true on a breakpoint or step stop.
 */
bool zeal_debugger_run(zeal_t*, unsigned long max_tstates);
#endif
