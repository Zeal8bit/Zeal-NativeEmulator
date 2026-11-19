/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hw/zeal.h"
#include "frontend/common/media.h"
#include "hostfs.h"
#include "frontend/common/desktop.h"
#include "frontend/common/app.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;

int desktop_run(bool frames, bool debug)
{
    int code = 0;
    if (frontend_init(&machine, frames, debug)) {
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

deinit:
    frontend_deinit(&machine);
    return code;
}
