/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/common/app.h"
#include "frontend/common/host.h"
#include "frontend/common/media.h"
#include "frontend/common/hostfs.h"
#include "utils/config.h"
#include "utils/log.h"
#if CONFIG_ENABLE_DEBUGGER
#include "frontend/common/symbols.h"
#endif

static void frontend_reset(void* userdata, zeal_t* machine, bool after_devices)
{
    (void)userdata;
    if (!after_devices) {
        frontend_reset_input(machine);
    } else if (config.arguments.uprog_filename) {
        flash_load_from_file(&machine->rom, config.images.rom, config.arguments.uprog_filename);
    }
}

int frontend_init(zeal_t* machine)
{
    bool debug = false;
    if (config.arguments.headless && !config.arguments.console && config.debugger.enabled == DEBUGGER_STATE_ARG) {
        config.arguments.console = true;
    }
#if CONFIG_ENABLE_DEBUGGER
    debug = config.arguments.console ? config.debugger.enabled == DEBUGGER_STATE_ARG :
        config_debugger_enabled() && !config.arguments.headless;
#endif
    frontend_open(debug);
    const zeal_options_t options = {
        .host = &frontend_host,
        .frames = !config.arguments.headless,
        .debugger = debug,
        .stop_on_reset = config.arguments.no_reset,
        .no_dma = config.arguments.no_dma,
#if ZVB_SCANLINE_RENDERING
        .no_scanline_rendering = config.arguments.no_scanline_rendering,
#endif
    };
    int err = zeal_init(machine, &options);
    if (err) return err;
    machine->reset_hook = frontend_reset;
#if CONFIG_ENABLE_DEBUGGER
    if (config.arguments.map_file) debugger_load_symbols(&machine->dbg, config.arguments.map_file);
    if (config.arguments.breakpoints) debugger_set_breakpoints_str(&machine->dbg, config.arguments.breakpoints);
#endif
    frontend_bind_input(machine);
    frontend_rtc_bind(&machine->rtc);
    if (!frontend_cf_init(&frontend_media, &machine->compactflash, config.arguments.cf_filename)) {
        machine->compactflash.parent.host = &frontend_host;
        mmu_register_io_device(&machine->cpu.mmu, 0x70, &machine->compactflash.parent);
    }
    err = frontend_eeprom_init(&frontend_media, &machine->eeprom, config.images.eeprom);
    if (err) return err;
    frontend_hostfs_bind(&frontend_hostfs, &machine->hostfs);
    if (options.frames) v_log_printf(1, "[RENDER] Blitter: software\n");
    return frontend_bind_output(machine);
}
