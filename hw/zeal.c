/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "hw/zeal.h"
#include "hw/host.h"
#include "utils/helpers.h"

#define TSTATE_NS (1000000000UL / CPUFREQ)

#define CHECK_ERR(err)  \
    do {                \
        if (err)        \
            return err; \
    } while (0)

#if CONFIG_ENABLE_DEBUGGER
int zeal_debugger_init(zeal_t*, dbg_t*);
/**
 * @brief Read a byte from memory for the debugger, so write-only areas can still be read
 */
static uint8_t debug_read_memory(zeal_t* machine, hwaddr virt_addr)
{
    const int phys_addr      = mmu_get_phys_addr(&machine->cpu.mmu, virt_addr);
    const map_entry_t entry  = machine->cpu.mmu.mem_mapping[phys_addr / MEM_SPACE_ALIGN];
    device_t* device         = entry.dev;
    const int start_addr     = entry.page_from * MMU_PAGE_SIZE;

    if (device) {
        return device->mem_region.debug_read ?
                    device->mem_region.debug_read(device, phys_addr - start_addr) :
                    device->mem_region.read(device, phys_addr - start_addr);
    }

    hw_log(machine->host, false, 0, "[INFO] No device replied to memory read: 0x%04x (PC @ 0x%04x)\n", phys_addr, machine->cpu.pc);
    return 0;
}
#endif

int zeal_init(zeal_t* machine, const zeal_options_t* options)
{
    if (!machine || !options) return 1;
    int err;
    memset(machine, 0, sizeof(*machine));
    machine->host = options->host;
    machine->stop_on_reset = options->stop_on_reset;
#if CONFIG_ENABLE_DEBUGGER
    machine->dbg_read_memory = debug_read_memory;
    machine->dbg_enabled = options->debugger;
    machine->dbg.running = true;
    zeal_debugger_init(machine, &machine->dbg);
    machine->dbg.host = options->host;
    machine->dbg_state = ST_RUNNING;
#endif
    z80_init(&machine->cpu);
    machine->cpu.mmu.parent.host = options->host;
    mmu_t* mmu = z80_get_mmu(&machine->cpu);

    err = flash_init(&machine->rom, &machine->cpu.clock, options->host);
    CHECK_ERR(err);

    err = ram_init(&machine->ram, options->host);
    CHECK_ERR(err);

    err = pio_init(machine, &machine->pio);
    CHECK_ERR(err);

    machine->uart.host = options->host;
    err = uart_init(&machine->uart, &machine->pio);
    CHECK_ERR(err);

    err = i2c_init(&machine->i2c_bus, &machine->pio);
    machine->i2c_bus.host = options->host;
    CHECK_ERR(err);

    err = keyboard_init(&machine->keyboard, &machine->cpu.clock, &machine->pio);
    CHECK_ERR(err);
    err = snes_adapter_init(&machine->snes_adapter, &machine->pio);
    CHECK_ERR(err);

    err = ds1307_init(&machine->rtc);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->rtc.parent);
    CHECK_ERR(err);

    err = at24c512_init(&machine->eeprom);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->eeprom.parent);
    CHECK_ERR(err);

    err = hostfs_init(&machine->hostfs, mmu);
    machine->hostfs.parent.host = options->host;
    CHECK_ERR(err);

    /* Initialize the semihosting device with CPU pointer for register access */
    err = semihost_init(&machine->semihost, &machine->cpu);
    CHECK_ERR(err);

    /* Register the devices in the memory space */
    mmu_register_mem_device(mmu, 0x000000, &machine->rom.parent);
    if (machine->rom.size < NOR_FLASH_SIZE_KB_MAX) {
        /* Create a mirror in the upper 256KB */
        mmu_register_mem_device(mmu, 0x040000, &machine->rom.parent);
    }

    mmu_register_mem_device(mmu, 0x080000, &machine->ram.parent);
    const zvb_config_t zvb_config = {
        .host = options->host,
        .rendering_enabled = options->frames,
        .dma_disabled = options->no_dma,
#if ZVB_SCANLINE_RENDERING
        .scanline_disabled = options->no_scanline_rendering,
#endif
        .pio = &machine->pio,
    };
    err = zvb_init(&machine->zvb, &machine->cpu.clock, &zvb_config, mmu, &machine->cpu.bus_requested);
    CHECK_ERR(err);
    mmu_register_mem_device(mmu, ZVB_PHYS_ADDR_BASE, DEVICE(&machine->zvb));
    /* Register the devices in the I/O space */
    mmu_register_io_device(mmu, 0x10, &machine->semihost.parent);
    mmu_register_io_device(mmu, 0x80, &machine->zvb.parent);
    mmu_register_io_device(mmu, 0xc0, &machine->hostfs.parent);
    mmu_register_io_device(mmu, 0xd0, &machine->pio.parent);
    mmu_register_io_device(mmu, 0xe0, &machine->keyboard.parent);
    mmu_register_io_device(mmu, 0xf0, &machine->cpu.mmu.parent);

    return 0;
}

int zeal_reset(zeal_t* machine)
{
    z80_reset(&machine->cpu);
    if (machine->reset_hook) machine->reset_hook(machine->reset_userdata, machine, false);
    device_reset(DEVICE(&machine->cpu.mmu));
    device_reset(DEVICE(&machine->pio));
    i2c_reset(&machine->i2c_bus, &machine->pio);
    device_reset(DEVICE(&machine->keyboard));
    device_reset(DEVICE(&machine->zvb));

    if (machine->reset_hook) machine->reset_hook(machine->reset_userdata, machine, true);

#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) {
        machine->dbg_state = ST_PAUSED;
    }
#endif // CONFIG_ENABLE_DEBUGGER
    return 0;
}

zeal_step_result_t zeal_step(zeal_t* machine, bool debug)
{
    if (machine->should_exit) return ZEAL_STEP_STOP;
    bool execute = true;
#if CONFIG_ENABLE_DEBUGGER
    if (debug) {
        execute = machine->dbg_state != ST_PAUSED;
        /* A step-over request sets a temporary breakpoint past the call. */
        if (machine->dbg_state == ST_REQ_STEP_OVER) {
            int size = z80_instruction_size(&machine->cpu);
            debugger_set_temporary_breakpoint(&machine->dbg, machine->cpu.pc + size);
            machine->dbg_state = ST_RUNNING;
        }
    }
#else
    (void) debug;
#endif
    zeal_step_result_t result = ZEAL_STEP_NONE;
    if (execute) {
        z80_step(&machine->cpu);
        if (machine->compactflash.failed) {
            machine->should_exit = true;
            return ZEAL_STEP_FATAL;
        }
#if CONFIG_ENABLE_DEBUGGER
        if (debug) {
            /* Stop on a single step or a breakpoint. */
            if (machine->dbg_state == ST_REQ_STEP || debugger_is_breakpoint_set(&machine->dbg, machine->cpu.pc)) {
                machine->dbg_state = ST_PAUSED;
                debugger_clear_breakpoint_if_temporary(&machine->dbg, machine->cpu.pc);
            }
            if (machine->dbg_state == ST_PAUSED) result |= ZEAL_STEP_DEBUG_STOP;
        } else
#endif
        /* PC is back to zero: stop on software reset when requested. */
        if (machine->stop_on_reset && machine->cpu.pc == 0) {
            hw_log(machine->host, false, 0, "[ZEAL] PC returned to 0x0000 after running (cyc=%lu), exiting\n", machine->cpu.cyc);
            machine->should_exit = true;
            return ZEAL_STEP_STOP;
        }
    }
    /* Finalize pixels at the instruction boundary, including DMA elapsed time.
     * Fetching the frame later never executes hardware or renders lazily. */
    if (machine->zvb.rendering_enabled && zvb_prepare_render(&machine->zvb)) {
        zvb_render(&machine->zvb);
        result |= ZEAL_STEP_FRAME;
    }
#if CONFIG_ENABLE_DEBUGGER
    else if (debug && machine->dbg_state == ST_PAUSED && machine->zvb.rendering_enabled) {
        zvb_force_render(&machine->zvb);
        result |= ZEAL_STEP_FRAME;
    }
#endif
    return result;
}

static zeal_run_result_t zeal_run_budget(zeal_t* machine, unsigned long max_tstates, bool debug)
{
    const uint64_t start = machine->cpu.clock.now_ns;
    zeal_step_result_t reason = ZEAL_STEP_NONE;
    while (!machine->should_exit) {
        reason = zeal_step(machine, debug) & (ZEAL_STEP_STOP | ZEAL_STEP_DEBUG_STOP | ZEAL_STEP_FATAL);
        if (reason != ZEAL_STEP_NONE) break;
        /* Stop once the requested number of T-states has been spent. */
        if (max_tstates && (machine->cpu.clock.now_ns - start) / TSTATE_NS >= max_tstates) break;
    }
    if (machine->should_exit && reason == ZEAL_STEP_NONE) reason = ZEAL_STEP_STOP;
#if CONFIG_ENABLE_DEBUGGER
    if (debug) machine->dbg_state = ST_PAUSED;
#endif
    return (zeal_run_result_t){(machine->cpu.clock.now_ns - start) / TSTATE_NS, reason};
}

#if CONFIG_ENABLE_DEBUGGER
bool zeal_debugger_run(zeal_t* machine, unsigned long max_tstates)
{
    if (machine->dbg_state == ST_PAUSED) machine->dbg_state = ST_RUNNING;
    return (zeal_run_budget(machine, max_tstates, true).reason & ZEAL_STEP_DEBUG_STOP) != 0;
}
#endif

zeal_run_result_t zeal_run(zeal_t* machine, unsigned long max_tstates)
{
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) {
        debugger_continue(&machine->dbg);
        return zeal_run_budget(machine, max_tstates, true);
    }
#endif
    return zeal_run_budget(machine, max_tstates, false);
}

zeal_run_result_t zeal_run_for_tstates(zeal_t* machine, unsigned long tstates)
{
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) return zeal_run(machine, tstates);
#endif
    if (tstates) return zeal_run(machine, tstates);
    return (zeal_run_result_t){0, ZEAL_STEP_NONE};
}

void zeal_exit(zeal_t* machine)
{
    machine->should_exit = true;
}

void zeal_deinit(zeal_t* machine)
{
    zvb_deinit(&machine->zvb);
    vtimer_cancel(&machine->keyboard.timer);
    fifo_deinit(&machine->keyboard.queue);
    vtimer_cancel(&machine->rom.timer);
#if CONFIG_ENABLE_DEBUGGER
    debugger_deinit(&machine->dbg);
#endif
#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    free(machine->rom.data);
    machine->rom.data = NULL;
#endif
#if CONFIG_RAM_DYNAMIC_ARRAY
    free(machine->ram.data);
    machine->ram.data = NULL;
#endif
    vtimer_init(&machine->cpu.clock);
}
