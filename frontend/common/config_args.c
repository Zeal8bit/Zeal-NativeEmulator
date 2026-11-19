/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <getopt.h>
#include <stdlib.h>
#include "frontend/common/cli.h"
#include <string.h>
#include "utils/config.h"
#include "utils/version.h"
#include "utils/log.h"
#include "utils/paths.h"
#include "hw/zvb/zvb.h"

static const char* module = "[CONFIG]";

config_t config ={
    .audio = {
        .volume = 100,
    },

    .arguments = {
        .rom_filename = NULL,
        .hostfs_path = ".",
        .no_reset = false,
        .verbose = 0,
#if CONFIG_PROFILE_RENDER
        .profile = false,
#endif
    },

    .debugger = {
        .enabled = false,
        .keyboard_passthru = false,
        .hex_upper = true,
        .width = -1,
        .height = -1,
        .x = -1,
        .y = -1,
    },

    .window = {
        .width = -1,
        .height = -1,
        .x = -1,
        .y = -1,
        .display = -1,
    },
};

void config_debug(void)
{
    if(config.arguments.verbose < 2) return;

    log_printf("%s == CONFIG ==\n", module);

    log_printf("%s \n", module);
    log_printf("%s === command line ===\n", module);
    log_printf("%s  rom_filename: %s\n", module, config.arguments.rom_filename);
    log_printf("%s   hostfs_path: %s\n", module, config.arguments.hostfs_path);
    log_printf("%s      map_file: %s\n", module, config.arguments.map_file);
    log_printf("%s debug_enabled: %s\n", module, config.debugger.enabled == DEBUGGER_STATE_ARG ? "True" : "False");
    log_printf("%s       verbose: %u\n", module, config.arguments.verbose);
    log_printf("%s      no_reset: %s\n", module, config.arguments.no_reset ? "True" : "False");
    log_printf("%s        no_dma: %s\n", module, config.arguments.no_dma ? "True" : "False");
#if CONFIG_PROFILE_RENDER
    log_printf("%s       profile: %s\n", module, config.arguments.profile ? "True" : "False");
#endif

    log_printf("%s \n", module);
    log_printf("%s === audio ===\n", module);
    log_printf("%s  volume: %d\n", module, config.audio.volume);

    log_printf("%s \n", module);
    log_printf("%s === debugger ===\n", module);
    log_printf("%s enabled: %s\n", module, config.debugger.enabled == DEBUGGER_STATE_CONFIG ? "True" : "False");

    log_printf("%s \n", module);
    log_printf("%s === window ===\n", module);
    log_printf("%s   width: %d\n", module, config.window.width);
    log_printf("%s  height: %d\n", module, config.window.height);
    log_printf("%s       x: %d\n", module, config.window.x);
    log_printf("%s       y: %d\n", module, config.window.y);
    log_printf("%s display: %d\n", module, config.window.display);

    log_printf("%s \n", module);
    log_printf("%s === debugger ===\n", module);
    log_printf("%s   width: %d\n", module, config.debugger.width);
    log_printf("%s  height: %d\n", module, config.debugger.height);
    log_printf("%s       x: %d\n", module, config.debugger.x);
    log_printf("%s       y: %d\n", module, config.debugger.y);
    log_printf("%s \n\n", module);
}

static int print_version(const char* progname)
{
    (void) progname;
    log_printf("%s %s\n", ZEAL_NATIVE_NAME, ZEAL_NATIVE_VERSION);
    exit(0);
}

static int usage(const char* progname, const frontend_cli_option_t* options, size_t count)
{
    log_printf("%s %s\n", ZEAL_NATIVE_NAME, ZEAL_NATIVE_VERSION);
    log_printf("Usage: %s [OPTIONS]\n", progname);
    log_printf("\nOptions:\n");
    for (size_t i = 0; i < count; i++) log_printf("%s\n", options[i].help);
    log_printf("  -r, --rom <file>                   Load ROM file\n");
    log_printf("  -u, --uprog <file>[,<addr>]        Load user program in romdisk at hex address\n");
    log_printf("  -e, --eeprom <file>                Load EEPROM file\n");
    log_printf("  -t, --tf <file>                    Load TF/SDcard file\n");
    log_printf("  -H, --hostfs <path>                Set host filesystem path\n");
    log_printf("  -m, --map <file>                   Load memory map file (for debugging)\n");
    log_printf("  -g, --debug                        Enable debug mode\n");
    log_printf("  -b, --brk <addr/sym>[,<addr/sym>]  Set breakpoints on boot (requires debug mode)\n");
    log_printf("  -q, --no-reset                     Exit emulator when a reset is detected\n");
    log_printf("      --no-dma                       Disconnect ZVB DMA bus handshake wiring\n");
#if ZVB_SCANLINE_RENDERING
    log_printf("      --no-scanline-rendering         Render whole frames instead of scanlines\n");
#endif
#if CONFIG_PROFILE_RENDER
    log_printf("      --profile                      Log aggregated render profiling data\n");
#endif
    log_printf("  -v, --verbose                      Verbose console output; repeat for more detail (-vvv)\n");
    log_printf("  -h, --help                         Show this help message\n");
    log_printf("  -V, --version                      Show version information\n");
    log_printf("\n");
    log_printf("Example:\n");
    log_printf("  %s --rom game.bin --map mem.map --debug\n", progname);

    exit(0);
}

static const char* resolve_image_path(const char* configured, const char* env_name,
                                      const char* relative_path, char path[PATH_MAX])
{
    if (configured != NULL) {
        return configured;
    }
    const char* env_path = getenv(env_name);
    if (env_path != NULL && env_path[0] != '\0' && path_exists(env_path)) {
        return env_path;
    }
    if (path_exists(relative_path)) {
        return relative_path;
    }
    const char* config_dir = get_config_dir();
    if (config_dir == NULL) {
        return NULL;
    }

    const int length = snprintf(path, PATH_MAX, "%s/%s", config_dir, relative_path);
    if (length < 0 || length >= PATH_MAX || !path_exists(path)) {
        return NULL;
    }
    return path;
}

void config_resolve_images(void)
{
    static char rom_path[PATH_MAX];
    static char eeprom_path[PATH_MAX];
    static char tf_path[PATH_MAX];
    config.images.rom = resolve_image_path(config.arguments.rom_filename,
        "ZEAL_NATIVE_ROM", "roms/default.img", rom_path);
    config.images.eeprom = resolve_image_path(config.arguments.eeprom_filename,
        "ZEAL_NATIVE_EEPROM", "roms/eeprom.img", eeprom_path);
    config.images.tf = resolve_image_path(config.arguments.tf_filename,
        "ZEAL_NATIVE_TF", "roms/tf.img", tf_path);
}

static void read_optional_number(unsigned long* number, int argc, char* argv[])
{
    if (optarg) {
        *number = strtoul(optarg, NULL, 0);
    } else if (optind < argc) {
        /* getopt_long only consumes attached optional arguments. */
        char* end;
        unsigned long value = strtoul(argv[optind], &end, 0);
        if (end != argv[optind] && *end == '\0') {
            *number = value;
            optind++;
        }
    }
}

int frontend_configure(int argc, char* argv[], const frontend_cli_option_t* options, size_t count)
{
    enum {
        OPT_NO_DMA = 256,
#if ZVB_SCANLINE_RENDERING
        OPT_NO_SCANLINE_RENDERING,
#endif
#if CONFIG_PROFILE_RENDER
        OPT_PROFILE,
#endif
    };

    int opt;

    static const struct option common_options[] = {
        {      "rom", required_argument, 0, 'r'},
        {   "eeprom", required_argument, 0, 'e'},
        {       "tf", required_argument, 0, 't'},
        {    "uprog", required_argument, 0, 'u'},
        {       "cf", required_argument, 0, 'C'},
        {   "hostfs", required_argument, 0, 'H'},
        {      "map", required_argument, 0, 'm'},
        {    "debug", optional_argument, 0, 'g'},
        {      "brk", required_argument, 0, 'b'},
        { "no-reset",       no_argument, 0, 'q'},
        {   "no-dma",       no_argument, 0, OPT_NO_DMA},
#if ZVB_SCANLINE_RENDERING
        { "no-scanline-rendering", no_argument, 0, OPT_NO_SCANLINE_RENDERING},
#endif
#if CONFIG_PROFILE_RENDER
        {  "profile",       no_argument, 0, OPT_PROFILE},
#endif
        {  "verbose",       no_argument, 0, 'v'},
        {    "help",        no_argument, 0, 'h'},
        { "version",        no_argument, 0, 'V'},
        {          0,                 0, 0,   0}
    };

    static const char common_short[] = "r:e:u:t:C:H:m:b:qgvhV";
    const size_t common_count = sizeof(common_options) / sizeof(common_options[0]) - 1;
    struct option long_options[common_count + count + 1];
    char short_options[sizeof(common_short) + count * 3];
    memcpy(long_options, common_options, sizeof(common_options));
    memcpy(short_options, common_short, sizeof(common_short));
    size_t pos = sizeof(common_short) - 1;
    for (size_t i = 0; i < count; i++) {
        const frontend_cli_option_t* option = &options[i];
        long_options[common_count + i] = (struct option){
            option->name, option->argument ? required_argument :
                          option->number ? optional_argument : no_argument, NULL, option->key
        };
        short_options[pos++] = option->key;
        if (option->argument || option->number) short_options[pos++] = ':';
        if (option->number) short_options[pos++] = ':';
    }
    short_options[pos] = '\0';
    long_options[common_count + count] = (struct option){0};

    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                config.arguments.rom_filename = optarg;
                break;
            case 'e':
                config.arguments.eeprom_filename = optarg;
                break;
            case 't':
                config.arguments.tf_filename = optarg;
                break;
            case 'u':
                config.arguments.uprog_filename = optarg;
                break;
            case 'C':
                config.arguments.cf_filename = optarg;
                break;
            case 'h':
                return usage(argv[0], options, count);
            case 'V':
                return print_version(argv[0]);
            case 'H':
                config.arguments.hostfs_path = optarg;
                break;
            case 'm':
                config.arguments.map_file = optarg;
                break;
            case 'g':
                // did user disable the debug mode (override config?)
                if(optarg == NULL) {
                    config.debugger.enabled = DEBUGGER_STATE_ARG;
                } else if(optarg[0] == '0' && optarg[1] == '\0') {
                    config.debugger.enabled = DEBUGGER_STATE_ARG_DISABLE;
                } else {
                    config.debugger.enabled = DEBUGGER_STATE_ARG;
                }
                break;
            case 'b':
                config.arguments.breakpoints = optarg;
                break;
            case 'v':
                if (config.arguments.verbose < UINT8_MAX) {
                    config.arguments.verbose++;
                }
                break;
            case 'q':
                config.arguments.no_reset = true;
                break;
            case OPT_NO_DMA:
                config.arguments.no_dma = true;
                break;
#if ZVB_SCANLINE_RENDERING
            case OPT_NO_SCANLINE_RENDERING:
                config.arguments.no_scanline_rendering = true;
                break;
#endif
#if CONFIG_PROFILE_RENDER
            case OPT_PROFILE:
                config.arguments.profile = true;
                break;
#endif
            case '?':
                return 1;
            default:
                for (size_t i = 0; i < count; i++) {
                    if (options[i].key != opt) continue;
                    if (options[i].argument) *options[i].argument = optarg;
                    if (options[i].flag) *options[i].flag = true;
                    if (options[i].number) read_optional_number(options[i].number, argc, argv);
                    break;
                }
        }
    }

    if (config.arguments.hostfs_path == NULL) {
        log_printf("No HostFS path specified.\n");
    }

    // Process non-option arguments, if needed
    for (int i = optind; i < argc; i++) {
        log_printf("Non-option argument: %s\n", argv[i]);
    }

    return 0;
}
