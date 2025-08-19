/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;

int main(int argc, char* argv[])
{
    int code = 0;
    code = parse_command_args(argc, argv);
    if(code != 0) return code;

    config_parse_file(config.arguments.config_path);
    if(config.arguments.verbose) config_debug();

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

    if (flash_load_from_file(&machine.rom, config.arguments.rom_filename,
                             config.arguments.uprog_filename)) {
        goto deinit;
    }

    if (hostfs_load_path(&machine.hostfs, config.arguments.hostfs_path)) {
        goto deinit;
    }

    if (config.arguments.tf_filename != NULL &&
        zvb_spi_load_tf_image(&machine.zvb.spi, config.arguments.tf_filename)) {
        goto deinit;
    }

    code = zeal_run(&machine);

    int saved = config_save();
    config_unload();
    if(!saved && code == 0) return saved; // ???

deinit:
    zvb_sound_deinit(&machine.zvb.sound);
    return code;
}
