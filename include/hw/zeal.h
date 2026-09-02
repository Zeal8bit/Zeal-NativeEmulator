/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
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
#include "utils/config.h"
#include "utils/vtimer.h"
#include "hw/userport/snes_adapter.h"

#include "hw/i2c.h"
#include "hw/i2c/ds1307.h"
#include "hw/i2c/at24c512.h"

#if CONFIG_ENABLE_DEBUGGER
#include "debugger/debugger_ui.h"
#endif

typedef uint8_t dev_idx_t;

/**
 * @brief Macros related to RayLib window
 */
#define WIN_VISIBLE_WIDTH   (ZVB_MAX_RES_WIDTH  * 2)
#define WIN_VISIBLE_HEIGHT  (ZVB_MAX_RES_HEIGHT * 2)
#define WIN_NAME            "Zeal 8-bit Computer"
#define WIN_LOG_LEVEL       LOG_WARNING

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

    /* Renderer */
    bool headless;
    bool should_exit;

    /* Misc features */
    zeal_hostfs_t hostfs;

    /* Host keyboard polling timer */
    vtimer_node_t host_keyb_timer;

    /* Debugger related */
#if CONFIG_ENABLE_DEBUGGER
    bool             dbg_enabled;
    dbg_state_t      dbg_state;
    dbg_t            dbg;
    struct dbg_ui_t* dbg_ui;
    uint8_t        (*dbg_read_memory)(struct zeal_t*, hwaddr addr);
#endif
};

typedef struct zeal_t zeal_t;


/**
 * @brief Initialize the Zeal 8-bit Computer virtual machine with optional parameters
 */
int zeal_init(zeal_t* machine);


/**
 * @brief Reset the Zeal 8-bit Computer
 *
 * Resets the CPU and ZVB
 */
int zeal_reset(zeal_t* machine);

/**
 * @brief Run the virtual machine, won't return until the emulation is terminated
 */
int zeal_run(zeal_t* machine);

/**
 * @brief Run the machine for a given number of Z80 T-states (console 'wait' command)
 */
void zeal_run_for_tstates(zeal_t* machine, unsigned long tstates);

/**
 * @brief Stop the virtual machine, and call CloseWindow()
 */
void zeal_exit(zeal_t* machine);

/**
 * @brief Quantize a scale factor to the next or previous 10% step.
 */
float zeal_scale_quantize_tenths(float scale, float step);

#if CONFIG_ENABLE_DEBUGGER

/**
 * @brief Enable Zeal Debugger view
 */
int zeal_debug_enable(zeal_t* machine);

/**
 * @brief Disable Zeal Debugger view
 */
int zeal_debug_disable(zeal_t* machine);

/**
 * @brief Toggle the Zeal Debugger view
 */
void zeal_debug_toggle(dbg_t *dbg);

/**
 * @brief Run the machine while the debugger state machine allows it (console debugger).
 * @return true if stopped by a breakpoint or step request, false otherwise.
 */
bool zeal_debugger_run(zeal_t* machine, unsigned long max_tstates);

#endif // CONFIG_ENABLE_DEBUGGER
