/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif

#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;

#ifndef PLATFORM_WEB
static void handle_interrupt(int signal_number)
{
    (void) signal_number;
    machine.should_exit = 1;
}
#endif

#ifdef PLATFORM_WEB
EMSCRIPTEN_KEEPALIVE
void zeal_exit_web(void)
{
    zvb_sound_deinit(&machine.zvb.sound);
    zeal_exit(&machine);
}

EMSCRIPTEN_KEEPALIVE
void zeal_flush_storage_web(void)
{
    flash_save_to_file(&machine.rom, config.arguments.rom_filename);
    fflush(NULL);
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
#ifndef PLATFORM_WEB
    void (*previous_sigint_handler)(int) = SIG_DFL;
    bool sigint_handler_installed = false;
#endif

    code = parse_command_args(argc, argv);
    if(code != 0) return code;

    config_parse_file(config.arguments.config_path);
    config_debug();

    if (config.arguments.hostfs_path == NULL) {
        log_printf("No HostFS path specified.\n");
    }

    // Process non-option arguments, if needed
    for (int i = optind; i < argc; i++) {
        log_printf("Non-option argument: %s\n", argv[i]);
    }

    code = zeal_init(&machine);
    if (code != 0) {
        log_err_printf("Error initializing the machine\n");
        goto cleanup;
    }

    if (flash_load_from_file(&machine.rom, config.arguments.rom_filename,
                             config.arguments.uprog_filename)) {
        code = 1;
        goto cleanup;
    }

#ifndef PLATFORM_WEB
    if (hostfs_load_path(&machine.hostfs, config.arguments.hostfs_path)) {
        code = 1;
        goto cleanup;
    }
#endif

    if (config.arguments.tf_filename != NULL &&
        zvb_spi_load_tf_image(&machine.zvb.spi, config.arguments.tf_filename)) {
        code = 1;
        goto cleanup;
    }

#ifndef PLATFORM_WEB
    previous_sigint_handler = signal(SIGINT, handle_interrupt);
    if (previous_sigint_handler == SIG_ERR) {
        log_perror("Failed to install SIGINT handler");
        code = 1;
        goto cleanup;
    }
    sigint_handler_installed = true;
#endif

    code = zeal_run(&machine);

    flash_save_to_file(&machine.rom, config.arguments.rom_filename);

    int saved = config_save();
    if(!saved && code == 0) code = saved; // ???

cleanup:
#ifndef PLATFORM_WEB
    if (sigint_handler_installed) {
        signal(SIGINT, previous_sigint_handler);
    }
#endif
    keyboard_deinit(&machine.keyboard);
    zvb_sound_deinit(&machine.zvb.sound);
    config_unload();
    return code;
}
