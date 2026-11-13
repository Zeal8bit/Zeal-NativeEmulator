/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef PLATFORM_WEB
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#endif

#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;

#ifdef PLATFORM_WEB
/* Only request work from JS. File I/O must run on the main Asyncify stack,
 * rather than re-entering it while Raylib is sleeping between frames. */
#define WEB_STORAGE_FLUSH 1
#define WEB_STORAGE_EXIT 2

static int web_storage_request;

// These macro bodies contain JavaScript, not C.
// clang-format off
EM_JS(int, web_uses_opfs, (), { return Module.imageStorage === 'opfs'; });
EM_JS(void, web_ready, (), { Module.onEmulatorReady?.(); });
EM_JS(void, web_storage_complete, (int error), { Module.onStorageComplete?.(error); });
// clang-format on

EMSCRIPTEN_KEEPALIVE
void zeal_exit_web(void)
{
    web_storage_request = WEB_STORAGE_EXIT;
}

EMSCRIPTEN_KEEPALIVE
void zeal_flush_storage_web(void)
{
    web_storage_request = WEB_STORAGE_FLUSH;
}

void zeal_service_web(void)
{
    const int request = web_storage_request;
    if (!request) return;
    web_storage_request = 0;
    int error = 0;
    if (flash_save_to_file(&machine.rom, config.images.rom) < 0) {
        error = errno ? errno : EIO;
    }
    if (fflush(NULL) != 0) {
        error = errno ? errno : EIO;
    }
    if (machine.eeprom.file && ferror(machine.eeprom.file)) {
        error = EIO;
    }
    if (machine.zvb.spi.tf.img && ferror(machine.zvb.spi.tf.img)) {
        error = EIO;
    }
    if (machine.eeprom.file && fsync(fileno(machine.eeprom.file)) < 0) {
        error = errno ? errno : EIO;
    }
    if (machine.zvb.spi.tf.img && fsync(fileno(machine.zvb.spi.tf.img)) < 0) {
        error = errno ? errno : EIO;
    }
    if (!error && request == WEB_STORAGE_EXIT) {
        zeal_exit(&machine);
    }
    web_storage_complete(error);
}

EMSCRIPTEN_KEEPALIVE
void zeal_debug_toggle_web(void)
{
#if CONFIG_ENABLE_DEBUGGER
    zeal_debug_toggle(&machine.dbg);
#endif
}

EMSCRIPTEN_KEEPALIVE
void zeal_snes_button_web(uint8_t button, int pressed)
{
    snes_adapter_set_virtual_button(&machine.snes_adapter, button, pressed != 0);
}

EMSCRIPTEN_KEEPALIVE
void zeal_snes_clear_web(void)
{
    snes_adapter_clear_virtual_buttons(&machine.snes_adapter);
}
#endif

int main(int argc, char* argv[])
{
    int code = 0;
#ifdef PLATFORM_WEB
    if (web_uses_opfs()) {
        const backend_t backend = wasmfs_create_opfs_backend();
        if (wasmfs_create_directory("/opfs", 0777, backend) != 0) {
            log_err_printf("Could not mount OPFS\n");
            return 1;
        }
    }
#endif
    code = parse_command_args(argc, argv);
    if (code != 0) return code;

    config_parse_file(config.arguments.config_path);
    config_resolve_images();
    config_debug();

    if (config.arguments.hostfs_path == NULL) {
        log_printf("No HostFS path specified.\n");
    }

    // Process non-option arguments, if needed
    for (int i = optind; i < argc; i++) {
        log_printf("Non-option argument: %s\n", argv[i]);
    }

    if (zeal_init(&machine)) {
        log_err_printf("Error initializing the machine\n");
        goto deinit;
    }
#ifdef PLATFORM_WEB
    if (config.images.eeprom && !machine.eeprom.file) {
        log_err_printf("Could not open configured EEPROM image for persistence\n");
        code = 1;
        goto deinit;
    }
#endif

    if (flash_load_from_file(&machine.rom, config.images.rom,
                             config.arguments.uprog_filename)) {
        goto deinit;
    }

#ifndef PLATFORM_WEB
    if (hostfs_load_path(&machine.hostfs, config.arguments.hostfs_path)) {
        goto deinit;
    }
#endif

    if (config.images.tf != NULL &&
        zvb_spi_load_tf_image(&machine.zvb.spi, config.images.tf)) {
        goto deinit;
    }

#ifdef PLATFORM_WEB
    web_ready();
#endif
    code = zeal_run(&machine);

    flash_save_to_file(&machine.rom, config.arguments.rom_filename);
#ifdef PLATFORM_WEB
    at24c512_deinit(&machine.eeprom);
    if (machine.zvb.spi.tf.img) {
        fclose(machine.zvb.spi.tf.img);
        machine.zvb.spi.tf.img = NULL;
    }
#endif

    int saved = config_save();
    config_unload();
    if (!saved && code == 0) return saved; // ???

deinit:
    zvb_sound_deinit(&machine.zvb.sound);
    return code;
}
