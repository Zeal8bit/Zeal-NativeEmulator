/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define RINI_VALUE_DELIMITER '='
/* Room for frontend-specific keys (e.g. SDL/FLTK layout, watches); rini
 * silently drops values beyond its capacity (default 128). */
#define RINI_MAX_VALUE_CAPACITY 512
#define RINI_IMPLEMENTATION

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/version.h"
#include "frontend/common/panels.h"
#include "hw/zvb/zvb.h"
#include "utils/paths.h"
#include "utils/log.h"

static const char* module = "[CONFIG]";

config_t config ={
    .audio = {
        .volume = 100,
    },

    .arguments = {
        .config_path = "zeal.ini",
        .project_path = "zeal.ini",
        .rom_filename = NULL,
        .hostfs_path = ".",
        .config_save = false,
        .no_reset = false,
        .headless = FRONTEND_HEADLESS,
        .console = false,
        .headless_run_ticks = 0,
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
    log_printf("%s   config_path: %s\n", module, config.arguments.config_path);
    log_printf("%s  project_path: %s\n", module, config.arguments.project_path);
    log_printf("%s  rom_filename: %s\n", module, config.arguments.rom_filename);
    log_printf("%s   hostfs_path: %s\n", module, config.arguments.hostfs_path);
    log_printf("%s      map_file: %s\n", module, config.arguments.map_file);
    log_printf("%s debug_enabled: %s\n", module, config.debugger.enabled == DEBUGGER_STATE_ARG ? "True" : "False");
    log_printf("%s     headless: %s\n", module, config.arguments.headless ? "True" : "False");
    log_printf("%s headless_run_ticks: %lu\n", module, config.arguments.headless_run_ticks);
    log_printf("%s     console: %s\n", module, config.arguments.console ? "True" : "False");
    log_printf("%s   config_save: %s\n", module, config.arguments.config_save ? "True" : "False");
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

int usage(const char* progname)
{
    log_printf("%s %s\n", ZEAL_NATIVE_NAME, ZEAL_NATIVE_VERSION);
    log_printf("Usage: %s [OPTIONS]\n", progname);
    log_printf("\nOptions:\n");
    log_printf("  -c, --config <file>                User settings (default ~/.zeal8bit/zeal.ini)\n");
    log_printf("  -P, --project <file>               Project settings (default ./zeal.ini)\n");
    log_printf("  -s, --save                         Save * arguments to the project settings\n");
    log_printf("  -r, --rom <file>                   * Load ROM file\n");
    log_printf("  -u, --uprog <file>[,<addr>]        Load user program in romdisk at hex address\n");
    log_printf("  -e, --eeprom <file>                Load EEPROM file\n");
    log_printf("  -t, --tf <file>                    Load TF/SDcard file\n");
    log_printf("  -H, --hostfs <path>                Set host filesystem path\n");
    log_printf("  -m, --map <file>                   Load memory map file (for debugging)\n");
    log_printf("  -g, --debug                        * Enable debug mode\n");
    log_printf("  -b, --brk <addr/sym>[,<addr/sym>]  * Set breakpoints on boot (requires debug mode)\n");
#if FRONTEND_HEADLESS
    log_printf("  -n, --headless [<tstates>]         Run without GUI (no window/input/rendering)\n");
    log_printf("                                     Optional tstates number to execute can be given\n");
    log_printf("  -o, --console                      Read control commands from stdin (implies headless)\n");
#else
    log_printf("  -n, --headless                     Moved to zeal-headless\n");
    log_printf("  -o, --console                      Moved to zeal-headless\n");
#endif
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

int parse_command_args(int argc, char* argv[])
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

    struct option long_options[] = {
        {   "config", required_argument, 0, 'c'},
        {  "project", required_argument, 0, 'P'},
        {      "rom", required_argument, 0, 'r'},
        {   "eeprom", required_argument, 0, 'e'},
        {       "tf", required_argument, 0, 't'},
        {    "uprog", required_argument, 0, 'u'},
        {       "cf", required_argument, 0, 'C'},
        {   "hostfs", required_argument, 0, 'H'},
        {      "map", required_argument, 0, 'm'},
        {    "debug", optional_argument, 0, 'g'},
        {      "brk", required_argument, 0, 'b'},
        { "headless", optional_argument, 0, 'n'},
        {  "console",       no_argument, 0, 'o'},
        { "no-reset",       no_argument, 0, 'q'},
        {   "no-dma",       no_argument, 0, OPT_NO_DMA},
#if ZVB_SCANLINE_RENDERING
        { "no-scanline-rendering", no_argument, 0, OPT_NO_SCANLINE_RENDERING},
#endif
#if CONFIG_PROFILE_RENDER
        {  "profile",       no_argument, 0, OPT_PROFILE},
#endif
        {     "save",       no_argument, 0, 's'},
        {  "verbose",       no_argument, 0, 'v'},
        {    "help",        no_argument, 0, 'h'},
        { "version",        no_argument, 0, 'V'},
        {          0,                 0, 0,   0}
    };

    const char* config_path = get_config_path();
    if(config_path) config.arguments.config_path = config_path;

    while ((opt = getopt_long(argc, argv, "c:P:r:e:u:t:C:H:m:b:n::oqsgvhV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config.arguments.config_path = optarg;
                break;
            case 'P':
                config.arguments.project_path = optarg;
                break;
            case 's':
                config.arguments.config_save = true;
                break;
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
                return usage(argv[0]);
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
            case 'n':
                config.arguments.headless = true;
                if (optarg != NULL) {
                    config.arguments.headless_run_ticks = strtoul(optarg, NULL, 0);
                } else if (optind < argc) {
                    /* Optional argument requires the long form to be followed by `=`, so
                     * --headless 500 is invalid as is, it needs `--headless=500`.
                     * This branch will parse the next parameter to allow `--headless 500` */
                    char* endptr = NULL;
                    unsigned long ticks = strtoul(argv[optind], &endptr, 0);
                    if (endptr != argv[optind] && endptr != NULL && *endptr == '\0') {
                        config.arguments.headless_run_ticks = ticks;
                        optind++;
                    }
                }
                break;
            case 'o':
                config.arguments.console = true;
                /* Console mode is headless by definition */
                config.arguments.headless = true;
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
                // Handle unknown options
                log_err_printf("[CONFIG] Unknown option -%c\n", optopt);
                return 1;
        }
    }

    return 0;
}

/* The user settings (config_path) hold what is shared by every program:
 * windows, layout, audio, preferences. The project settings (project_path,
 * ./zeal.ini by default) hold what belongs to the program being developed:
 * image paths saved with -s, breakpoints, watches, map files... Values are
 * looked up in the project first, so a project can also override a user
 * setting. Both point to the same file when -c and -P name the same path. */

static bool project_is_user(void)
{
    return path_same_file(config.arguments.project_path, config.arguments.config_path) != 0;
}

static const char* lookup_text(const char* key, const char* fallback)
{
    const char* value = NULL;
    if (config.project_ini.values != NULL) {
        value = rini_get_config_value_text_fallback(config.project_ini, key, NULL);
    }
    if (value == NULL && config.ini.values != NULL) {
        value = rini_get_config_value_text_fallback(config.ini, key, NULL);
    }
    return value ? value : fallback;
}

static int lookup_int(const char* key, int fallback)
{
    if (config.project_ini.values != NULL && rini_get_config_value_text_fallback(config.project_ini, key, NULL)) {
        return rini_get_config_value_fallback(config.project_ini, key, fallback);
    }
    if (config.ini.values != NULL) return rini_get_config_value_fallback(config.ini, key, fallback);
    return fallback;
}

/* rini loads at most RINI_MAX_VALUE_CAPACITY values per file and drops the
 * rest silently. Remember when a file was cut, so that saving does not
 * replace it with the truncated copy. Same line rule as rini_load_config. */
static bool user_truncated, project_truncated;

static unsigned count_value_lines(const char* path)
{
    unsigned count = 0;
    FILE* f = fopen(path, "rt");
    if (f == NULL) return 0;
    char line[RINI_MAX_LINE_SIZE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != RINI_LINE_COMMENT_DELIMITER && line[0] != RINI_LINE_SECTION_DELIMITER &&
            line[0] != '\n' && line[0] != '\0') count++;
    }
    fclose(f);
    return count;
}

static bool loads_partially(const char* path)
{
    const unsigned count = count_value_lines(path);
    if (count <= RINI_MAX_VALUE_CAPACITY) return false;
    log_err_printf("%s %s has %u settings, only %d were loaded: it will not be saved over\n",
                   module, path_sanitize(path), count, RINI_MAX_VALUE_CAPACITY);
    return true;
}

void config_parse_file(const char* file) {
    log_printf("%s %s\n", module, path_sanitize(file));
    if(path_exists(file)) {
        config.ini = rini_load_config(file);
        user_truncated = loads_partially(file);
    }

    const char* project = config.arguments.project_path;
    if (project != NULL && path_exists(project) && !path_same_file(project, file)) {
        log_printf("%s project %s\n", module, path_sanitize(project));
        config.project_ini = rini_load_config(project);
        project_truncated = loads_partially(project);
    }

    if(config.arguments.rom_filename == NULL) {
        config.arguments.rom_filename = lookup_text("ROM_FILENAME", NULL);
    }
    if(config.arguments.eeprom_filename == NULL) {
        config.arguments.eeprom_filename = lookup_text("EEPROM_FILENAME", NULL);
    }
    if(config.arguments.tf_filename == NULL) {
        config.arguments.tf_filename = lookup_text("TF_FILENAME", NULL);
    }

    config.debugger.config_enabled = lookup_int("DEBUG_ENABLED", DEBUGGER_STATE_DISABLED);
    if(config.debugger.enabled != DEBUGGER_STATE_ARG && config.debugger.enabled != DEBUGGER_STATE_ARG_DISABLE)
        config.debugger.enabled = config.debugger.config_enabled;

    config.audio.volume = lookup_int("AUDIO_VOLUME", 100);
    if (config.audio.volume < 0) {
        config.audio.volume = 0;
    } else if (config.audio.volume > 100) {
        config.audio.volume = 100;
    }

    const char* theme = config.ini.values ?
        rini_get_config_value_text_fallback(config.ini, "FLTK_THEME", "") : "";
    config.window.theme = strcmp(theme, "light") == 0 ? THEME_LIGHT :
                          strcmp(theme, "dark") == 0 ? THEME_DARK : THEME_SYSTEM;

    config.window.width = lookup_int("WIN_WIDTH", -1);
    config.window.height = lookup_int("WIN_HEIGHT", -1);
    config.window.x = lookup_int("WIN_POS_X", -1);
    config.window.y = lookup_int("WIN_POS_Y", -1);
    config.window.display = lookup_int("WIN_DISPLAY", -1);

    config.debugger.width = lookup_int("DEBUG_WIDTH", -1);
    config.debugger.height = lookup_int("DEBUG_HEIGHT", -1);
    config.debugger.x = lookup_int("DEBUG_POS_X", -1);
    config.debugger.y = lookup_int("DEBUG_POS_Y", -1);
    config.debugger.hex_upper = lookup_int("DEBUG_HEX_UPPER", 1);
}

config_save_hook_t config_save_hook = NULL;

#define MAX_OWNED_PREFIXES 16
static const char* owned_prefixes[MAX_OWNED_PREFIXES];
static int owned_count;

void config_own_project_keys(const char* prefix)
{
    for (int i = 0; i < owned_count; i++) {
        if (strcmp(owned_prefixes[i], prefix) == 0) return;
    }
    if (owned_count < MAX_OWNED_PREFIXES) owned_prefixes[owned_count++] = prefix;
}

static bool owned_key(const char* key)
{
    for (int i = 0; i < owned_count; i++) {
        if (strncmp(key, owned_prefixes[i], strlen(owned_prefixes[i])) == 0) return true;
    }
    return false;
}

static bool has_key(const rini_config* ini, const char* key)
{
    for (unsigned i = 0; i < ini->count; i++) {
        if (strcmp(ini->values[i].key, key) == 0) return true;
    }
    return false;
}

static void remove_key(rini_config* ini, const char* key)
{
    for (unsigned i = 0; i < ini->count; i++) {
        if (strcmp(ini->values[i].key, key) == 0) {
            memmove(&ini->values[i], &ini->values[i + 1], (ini->count - i - 1) * sizeof(ini->values[0]));
            ini->count--;
            return;
        }
    }
}

/* Keys loaded from the project file that nothing wrote this time: keep
 * them unless the frontend manages them (config_own_project_keys). A kept
 * key that is also a user setting is a project override: its value was
 * only read for this project, so the user file keeps its own value. */
static void keep_project_keys(rini_config* ini, rini_config* project)
{
    if (config.project_ini.values == NULL) return;
    for (unsigned i = 0; i < config.project_ini.count; i++) {
        const rini_config_value* v = &config.project_ini.values[i];
        if (v->key[0] == '\0' || has_key(project, v->key) || owned_key(v->key)) continue;
        rini_set_config_value_text(project, v->key, v->text, v->desc);
        if (has_key(ini, v->key)) {
            const char* user = config.ini.values ?
                rini_get_config_value_text_fallback(config.ini, v->key, NULL) : NULL;
            if (user) rini_set_config_value_text(ini, v->key, user, "");
            else remove_key(ini, v->key);
        }
    }
}

/* Room in the files being written: what can be loaded back plus headroom
 * for comment lines and anything beyond, so a file that would hold more
 * values than the loader reads is seen (counted, or filling the output)
 * and rejected by can_write() instead of being cut. */
#define OUTPUT_CAPACITY (RINI_MAX_VALUE_CAPACITY + 128)

static rini_config new_output(void)
{
    rini_config out = { 0 };
    out.values = calloc(OUTPUT_CAPACITY, sizeof(*out.values));
    if (out.values != NULL) out.capacity = OUTPUT_CAPACITY;
    return out;
}

/* A file is only replaced when the loader will read all of it back and
 * the file being replaced was itself read in full; otherwise it is kept
 * unchanged and the save fails. */
static bool can_write(const rini_config* out, const char* path, bool truncated)
{
    unsigned values = 0;
    for (unsigned i = 0; i < out->count; i++) {
        if (out->values[i].key[0] != '\0') values++;
    }
    if (truncated) {
        log_err_printf("%s %s was only partly loaded: not saved, the file is unchanged\n",
                       module, path_sanitize(path));
        return false;
    }
    if (out->count >= out->capacity || values > RINI_MAX_VALUE_CAPACITY) {
        log_err_printf("%s %s would hold %u settings, more than the %d that can be loaded: not saved, the file is unchanged\n",
                       module, path_sanitize(path), values, RINI_MAX_VALUE_CAPACITY);
        return false;
    }
    return true;
}

/* A value that is a plain integer is written as is, anything else quoted
 * without padding (rini's writer pads short strings inside the quotes) */
static bool is_plain_int(const char* text)
{
    const char* p = (*text == '-') ? text + 1 : text;
    if (*p == '\0') return false;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

/* rini reads a quoted value up to the first quote followed by spaces and a
 * description delimiter or the end of the line: a value containing such a
 * sequence would come back cut, so it is not written. */
static bool quoted_readable(const char* text)
{
    for (const char* q = strchr(text, '"'); q != NULL; q = strchr(q + 1, '"')) {
        const char* after = q + 1;
        while (*after == ' ') after++;
        if (*after == '\0' || *after == '#') return false;
    }
    return true;
}

/**
 * Write `ini` to `path` in place of rini_save_config (which reports
 * nothing): the new contents go to a temporary file in the same directory,
 * which replaces the file only once it was completely written, so a failure
 * leaves the previous file and its permissions intact. A symbolic link
 * keeps pointing to the file it named. A file with several names (hard
 * links) cannot be replaced for all of them at once: it is not written.
 * Returns false (with a message naming the file) on error.
 */
static bool write_ini(const rini_config* ini, const char* path)
{
    char target[PATH_MAX];
    char tmp[PATH_MAX + 32];
    struct stat st;
    FILE* f = NULL;
    bool ok = true;

    if (realpath(path, target) == NULL) {
        if (errno != ENOENT || strlen(path) >= sizeof(target)) goto fail;
#ifndef _WIN32
        /* A symbolic link to a missing file: replacing the link itself
         * would silently detach it */
        if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
            log_err_printf("%s %s is a link to a missing file: not saved\n", module, path_sanitize(path));
            return false;
        }
#endif
        strcpy(target, path);   /* new file */
    }
    if (stat(target, &st) == 0) {
        /* Only an ordinary file the user may write is replaced: not a
         * read-only file, a FIFO or a device, even in a writable folder,
         * and not a file with several names (hard links), which cannot all
         * be replaced at once */
        const char* why = !S_ISREG(st.st_mode) ? "is not a regular file"
                        : access(target, W_OK) != 0 ? "is not writable"
                        : st.st_nlink > 1 ? "has several hard links" : NULL;
        if (why) {
            log_err_printf("%s %s %s: not saved, the file is unchanged\n", module, path_sanitize(path), why);
            return false;
        }
    }
    f = path_create_temp(target, tmp, sizeof(tmp));
    if (f == NULL) goto fail;
    for (unsigned i = 0; ok && i < ini->count; i++) {
        const rini_config_value* v = &ini->values[i];
        int n;
        if (v->key[0] == '\0') {
            n = v->desc[0] ? fprintf(f, "# %s\n", v->desc) : fprintf(f, "#\n");
        } else if (strpbrk(v->text, "\r\n") != NULL || strlen(v->text) > ZEAL_INI_VALUE_MAX ||
                   (!is_plain_int(v->text) && !quoted_readable(v->text))) {
            /* Cannot be read back as is (possibly cut when it was set) */
            log_err_printf("%s %s: %s cannot be saved as is\n", module, path_sanitize(path), v->key);
            errno = EINVAL;
            n = -1;
        } else if (is_plain_int(v->text)) {
            n = fprintf(f, "%-28s = %6s      # %s\n", v->key, v->text, v->desc);
        } else {
            n = fprintf(f, "%-28s = \"%s\"      # %s\n", v->key, v->text, v->desc);
        }
        ok = n >= 0;
    }
    if (ok) ok = path_commit(f) == 0;
    {
        const int err = errno;
        if (fclose(f) != 0) ok = false;
        else errno = err;
    }
    if (ok && path_replace(tmp, target) == 0) return true;
    {
        const int err = errno;
        remove(tmp);
        errno = err;
    }
fail:
    log_err_printf("%s Could not save %s: %s\n", module, path_sanitize(path), strerror(errno));
    return false;
}

static bool has_values(const rini_config* ini)
{
    for (unsigned i = 0; i < ini->count; i++) {
        if (ini->values[i].key[0] != '\0') return true;
    }
    return false;
}

static int save_files(bool save_project)
{
    int result = 0;
    rini_config ini = new_output();
    rini_config project = new_output();
    if (!ini.values || !project.values) {
        log_err_printf("%s Could not allocate configuration output\n", module);
        rini_unload_config(&ini);
        rini_unload_config(&project);
        return -1;
    }
    rini_set_config_comment_line(&project, NULL);
    rini_set_config_comment_line(&project, "Zeal Native Emulator: project settings");
    rini_set_config_comment_line(&project, NULL);

    // main header
    rini_set_config_comment_line(&ini, NULL);
    rini_set_config_comment_line(&ini, "Zeal Native Emulator");
    rini_set_config_comment_line(&ini, NULL);

    if(config.arguments.config_save) {
        // config.arguments header
        config_arguments_t *args = &config.arguments;
        rini_set_config_comment_line(&project, "Arguments");
        if(args->rom_filename != NULL)
            rini_set_config_value_text(&project, "ROM_FILENAME", args->rom_filename, "ROM Filename");
        if(args->eeprom_filename != NULL)
            rini_set_config_value_text(&project, "EEPROM_FILENAME", args->eeprom_filename, "EEPROM Filename");
        if(args->tf_filename != NULL)
            rini_set_config_value_text(&project, "TF_FILENAME", args->tf_filename, "TF/SD Card Filename");

        if(config.debugger.enabled == DEBUGGER_STATE_ARG) {
            config.debugger.config_enabled = true;
        } else if(config.debugger.enabled == DEBUGGER_STATE_ARG_DISABLE) {
            config.debugger.enabled = false;
        }
    }

    config_window_t *window = &config.window;
    rini_set_config_comment_line(&ini, "Audio");
    rini_set_config_value(&ini, "AUDIO_VOLUME", config.audio.volume, "Master Volume Percent");

    rini_set_config_comment_line(&ini, "Main Window");
    rini_set_config_value(&ini, "WIN_WIDTH", window->width, "Width");
    rini_set_config_value(&ini, "WIN_HEIGHT", window->height, "Height");
    rini_set_config_value(&ini, "WIN_POS_X", window->x, "X Position");
    rini_set_config_value(&ini, "WIN_POS_Y", window->y, "Y Position");
    rini_set_config_value(&ini, "WIN_DISPLAY", window->display, "Display Number");

#if CONFIG_ENABLE_DEBUGGER
    config_debugger_t *debugger = &config.debugger;
    rini_set_config_comment_line(&ini, "Debugger");
    rini_set_config_value(&ini, "DEBUG_WIDTH", debugger->width, "Width");
    rini_set_config_value(&ini, "DEBUG_HEIGHT", debugger->height, "Height");
    rini_set_config_value(&ini, "DEBUG_POS_X", debugger->x, "X Position");
    rini_set_config_value(&ini, "DEBUG_POS_Y", debugger->y, "Y Position");
    rini_set_config_value(&ini, "DEBUG_ENABLED", debugger->config_enabled, "Debug Enabled");
    rini_set_config_value(&ini, "DEBUG_HEX_UPPER", debugger->hex_upper, "Use Upper Hex");

    config_panels_save(&ini);
#endif // CONFIG_ENABLE_DEBUGGER

    if (config_save_hook) config_save_hook(&ini, &project);

    /* Saved image paths are kept, without -s, in the file they were loaded
     * from: the project file, or the user file (older configurations, and
     * the one-file setup where -c and -P name the same file). */
    if (!config.arguments.config_save) {
        static const char* const kept[] = { "ROM_FILENAME", "EEPROM_FILENAME", "TF_FILENAME" };
        for (unsigned i = 0; i < sizeof(kept) / sizeof(kept[0]); i++) {
            const char* value = config.project_ini.values ?
                rini_get_config_value_text_fallback(config.project_ini, kept[i], NULL) : NULL;
            if (value) {
                rini_set_config_value_text(&project, kept[i], value, "");
                continue;
            }
            value = config.ini.values ? rini_get_config_value_text_fallback(config.ini, kept[i], NULL) : NULL;
            if (value) rini_set_config_value_text(&ini, kept[i], value, "");
        }
    }

    keep_project_keys(&ini, &project);

    /* The appearance is always a user preference, even if a separate
     * project file happens to contain the same key. */
    if (window->theme != THEME_SYSTEM) {
        rini_set_config_value_text(&ini, "FLTK_THEME", window->theme == THEME_LIGHT ? "light" : "dark",
                                   "SDL/FLTK appearance override");
    }

    if (project_is_user()) {
        /* One file for both: append the project values */
        for (unsigned i = 0; i < project.count; i++) {
            const rini_config_value* v = &project.values[i];
            if (v->key[0] != '\0') rini_set_config_value_text(&ini, v->key, v->text, v->desc);
        }
    } else if (save_project && (has_values(&project) || path_exists(config.arguments.project_path))) {
        /* Never create a project file in a directory with nothing to save */
        if (!can_write(&project, config.arguments.project_path, project_truncated) ||
            !write_ini(&project, config.arguments.project_path)) {
            result = -1;
        }
    }
    if (!can_write(&ini, config.arguments.config_path, user_truncated) ||
        !write_ini(&ini, config.arguments.config_path)) {
        result = -1;
    }
    rini_unload_config(&ini);
    rini_unload_config(&project);

    return result;
}

int config_save(void)
{
    return save_files(true);
}

int config_save_user(void)
{
    return save_files(false);
}

void config_unload(void)
{
    if(config.ini.values != NULL) {
        rini_unload_config(&config.ini);
    }
    if(config.project_ini.values != NULL) {
        rini_unload_config(&config.project_ini);
    }
}

int config_get(const char *key, int defaultValue) {
    return lookup_int(key, defaultValue);
}
const char* config_get_text(const char *key, const char *defaultValue) {
    return lookup_text(key, defaultValue);
}

void config_set(const char *key, int value, const char *desc) {
    rini_set_config_value(&config.ini, key, value, desc);
}
void config_set_text(const char *key, const char *value, const char *desc) {
    rini_set_config_value_text(&config.ini, key, value, desc);
}
