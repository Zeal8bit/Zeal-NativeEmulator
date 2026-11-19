/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include "hw/zeal.h"
#include "frontend/common/media.h"
#include "frontend/common/app.h"
#include "frontend/wasm/frontend.h"
#include "frontend/wasm/snes_adapter.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;

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
    if (frontend_media.eeprom && ferror(frontend_media.eeprom)) {
        error = EIO;
    }
    if (frontend_media.tf && ferror(frontend_media.tf)) {
        error = EIO;
    }
    if (frontend_media.eeprom && fsync(fileno(frontend_media.eeprom)) < 0) {
        error = errno ? errno : EIO;
    }
    if (frontend_media.tf && fsync(fileno(frontend_media.tf)) < 0) {
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
    /* Compatibility export: the browser frontend has no debugger UI. */
}

EMSCRIPTEN_KEEPALIVE
void zeal_snes_button_web(uint8_t button, int pressed)
{
    snes_adapter_set_virtual_button(&frontend_snes, button, pressed != 0);
}

EMSCRIPTEN_KEEPALIVE
void zeal_snes_clear_web(void)
{
    snes_adapter_clear_virtual_buttons(&frontend_snes);
}

int main(int argc, char* argv[])
{
    int code = 0;
    if (web_uses_opfs()) {
        const backend_t backend = wasmfs_create_opfs_backend();
        if (wasmfs_create_directory("/opfs", 0777, backend) != 0) {
            log_err_printf("Could not mount OPFS\n");
            return 1;
        }
    }
    code = frontend_configure(argc, argv);
    if (code != 0) return code;

    if (frontend_init(&machine, false)) {
        log_err_printf("Error initializing the machine\n");
        goto deinit;
    }
    if (config.images.eeprom && !frontend_media.eeprom) {
        log_err_printf("Could not open configured EEPROM image for persistence\n");
        code = 1;
        goto deinit;
    }

    if (flash_load_from_file(&machine.rom, config.images.rom,
                             config.arguments.uprog_filename)) {
        goto deinit;
    }

    if (config.images.tf != NULL &&
        frontend_tf_load(&frontend_media, &machine.zvb.spi, config.images.tf)) {
        goto deinit;
    }

    web_ready();
    code = frontend_run(&machine);
    if (machine.compactflash.failed) goto deinit;

    flash_save_to_file(&machine.rom, config.arguments.rom_filename);
    frontend_eeprom_close(&frontend_media);
    frontend_tf_close(&frontend_media);

deinit:
    frontend_deinit(&machine);
    return code;
}
