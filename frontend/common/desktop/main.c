/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


#include "hw/zeal.h"
#include "frontend/common/media.h"
#include "hostfs.h"
#include "frontend/common/app.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;


int main(int argc, char* argv[])
{
    int code = 0;
    code = frontend_configure(argc, argv);
    if (code != 0) return code;

    if (frontend_init(&machine, true)) {
        log_err_printf("Error initializing the machine\n");
        goto deinit;
    }

    if (flash_load_from_file(&machine.rom, config.images.rom,
                             config.arguments.uprog_filename)) {
        goto deinit;
    }

    if (frontend_hostfs_load_path(config.arguments.hostfs_path)) {
        goto deinit;
    }

    if (config.images.tf != NULL &&
        frontend_tf_load(&frontend_media, &machine.zvb.spi, config.images.tf)) {
        goto deinit;
    }

    code = frontend_run(&machine);
    if (machine.compactflash.failed) goto deinit;

    flash_save_to_file(&machine.rom, config.arguments.rom_filename);

    int saved = config_save();
    config_unload();
    (void) saved;

deinit:
    frontend_deinit(&machine);
    return code;
}
