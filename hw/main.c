/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"

static zeal_t machine;
static char run_hostfs_root[PATH_MAX];
static char run_guest_path[PATH_MAX];

static int validate_run_path(void)
{
    char file[PATH_MAX];
    char joined[PATH_MAX];
    struct stat st;
    const char* run = config.arguments.run_filename;

    if (run == NULL) {
        return 0;
    }

    if (!config.arguments.hostfs_explicit) {
        if (realpath(run, file) == NULL) {
            log_perror("[CONFIG] Could not resolve --run path");
            return 1;
        }

        if (stat(file, &st) != 0 || !S_ISREG(st.st_mode) || access(file, R_OK) != 0) {
            log_err_printf("[CONFIG] --run path must name a readable regular file\n");
            return 1;
        }

        char* separator = strrchr(file, '/');
#ifdef _WIN32
        char* backslash = strrchr(file, '\\');
        if (backslash != NULL && (separator == NULL || backslash > separator)) {
            separator = backslash;
        }
#endif
        if (separator == NULL || separator[1] == '\0') {
            log_err_printf("[CONFIG] Could not split --run path\n");
            return 1;
        }

        const size_t root_length = (size_t) (separator - file);
        if (root_length == 0) {
            snprintf(run_hostfs_root, sizeof(run_hostfs_root), "%c", file[0]);
        } else {
            memcpy(run_hostfs_root, file, root_length);
            run_hostfs_root[root_length] = '\0';
        }
        snprintf(run_guest_path, sizeof(run_guest_path), "%s", separator + 1);

        config.arguments.hostfs_path = run_hostfs_root;
        config.arguments.run_filename = run_guest_path;
    } else {
        if (run[0] == '\0' || run[0] == '/' ||
            (run[0] != '\0' && run[1] == ':')) {
            log_err_printf("[CONFIG] --run path must be relative to explicit HostFS root\n");
            return 1;
        }

        if (snprintf(joined, sizeof(joined), "%s/%s",
                     config.arguments.hostfs_path, run) >= (int) sizeof(joined)) {
            log_err_printf("[CONFIG] --run path is too long\n");
            return 1;
        }

        if (realpath(config.arguments.hostfs_path, run_hostfs_root) == NULL ||
            realpath(joined, file) == NULL) {
            log_perror("[CONFIG] Could not resolve --run path");
            return 1;
        }

        const size_t root_len = strlen(run_hostfs_root);
        const bool root_has_separator =
            root_len > 0 && (run_hostfs_root[root_len - 1] == '/' ||
                             run_hostfs_root[root_len - 1] == '\\');
        if (strncmp(run_hostfs_root, file, root_len) != 0 ||
            (!root_has_separator && file[root_len] != '\0' &&
             file[root_len] != '/' && file[root_len] != '\\')) {
            log_err_printf("[CONFIG] --run path escapes HostFS root\n");
            return 1;
        }

        if (stat(file, &st) != 0 || !S_ISREG(st.st_mode) || access(file, R_OK) != 0) {
            log_err_printf("[CONFIG] --run path must name a readable regular file\n");
            return 1;
        }

        snprintf(run_guest_path, sizeof(run_guest_path), "%s", run);
        for (char* p = run_guest_path; *p != '\0'; p++) {
            if (*p == '\\') {
                *p = '/';
            }
        }
        config.arguments.run_filename = run_guest_path;
    }

    return 0;
}

static void handle_interrupt(int signal_number)
{
    (void)signal_number;
    machine.should_exit = 1;
}

int main(int argc, char* argv[])
{
    int code = 0;
    code = parse_command_args(argc, argv);
    if(code != 0) return code;

    config_parse_file(config.arguments.config_path);
    if (validate_run_path()) {
        return 1;
    }
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

    code = flash_load_from_file(&machine.rom, config.arguments.rom_filename,
                                config.arguments.uprog_filename,
                                config.arguments.run_filename);
    if (code != 0) {
        goto deinit;
    }

    if (hostfs_load_path(&machine.hostfs, config.arguments.hostfs_path)) {
        goto deinit;
    }

    if (config.arguments.tf_filename != NULL &&
        zvb_spi_load_tf_image(&machine.zvb.spi, config.arguments.tf_filename)) {
        goto deinit;
    }

    if (signal(SIGINT, handle_interrupt) == SIG_ERR) {
        log_err_printf("Failed to install SIGINT handler\n");
        goto deinit;
    }

    code = zeal_run(&machine);

    flash_save_to_file(&machine.rom, config.arguments.rom_filename);

    int saved = config_save();
    config_unload();
    if(!saved && code == 0) return saved; // ???

deinit:
    zvb_sound_deinit(&machine.zvb.sound);
    return code;
}
