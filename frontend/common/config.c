/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define RINI_VALUE_DELIMITER '='
/* Keep the existing INI load limit for headless and WASM. */
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
#include "utils/config_ini.h"
#include "hw/zvb/zvb.h"
#include "utils/paths.h"
#include "utils/log.h"

static rini_config user_ini, project_ini;
static const char* module = "[CONFIG]";

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
    if (project_ini.values != NULL) {
        value = rini_get_config_value_text_fallback(project_ini, key, NULL);
    }
    if (value == NULL && user_ini.values != NULL) {
        value = rini_get_config_value_text_fallback(user_ini, key, NULL);
    }
    return value ? value : fallback;
}

static int lookup_int(const char* key, int fallback)
{
    if (project_ini.values != NULL && rini_get_config_value_text_fallback(project_ini, key, NULL)) {
        return rini_get_config_value_fallback(project_ini, key, fallback);
    }
    if (user_ini.values != NULL) return rini_get_config_value_fallback(user_ini, key, fallback);
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

int config_parse_file(const char* file) {
    log_printf("%s %s\n", module, path_sanitize(file));
    if(path_exists(file)) {
        user_ini = rini_load_config(file);
        user_truncated = loads_partially(file);
    }

    const char* project = config.arguments.project_path;
    if (project != NULL && path_exists(project) && !path_same_file(project, file)) {
        log_printf("%s project %s\n", module, path_sanitize(project));
        project_ini = rini_load_config(project);
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

    const char* theme = user_ini.values ?
        rini_get_config_value_text_fallback(user_ini, "FLTK_THEME", "") : "";
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
    return 0;
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
    if (project_ini.values == NULL) return;
    for (unsigned i = 0; i < project_ini.count; i++) {
        const rini_config_value* v = &project_ini.values[i];
        if (v->key[0] == '\0' || has_key(project, v->key) || owned_key(v->key)) continue;
        rini_set_config_value_text(project, v->key, v->text, v->desc);
        if (has_key(ini, v->key)) {
            const char* user = user_ini.values ?
                rini_get_config_value_text_fallback(user_ini, v->key, NULL) : NULL;
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

static bool write_values(FILE* f, const void* data)
{
    const rini_config* ini = data;
    bool ok = true;
    for (unsigned i = 0; ok && i < ini->count; i++) {
        const rini_config_value* v = &ini->values[i];
        int n;
        if (v->key[0] == '\0') {
            n = v->desc[0] ? fprintf(f, "# %s\n", v->desc) : fprintf(f, "#\n");
        } else if (strpbrk(v->text, "\r\n") != NULL || strlen(v->text) > ZEAL_INI_VALUE_MAX ||
                   (!is_plain_int(v->text) && !quoted_readable(v->text))) {
            /* Cannot be read back as is (possibly cut when it was set) */
            log_err_printf("%s %s cannot be saved as is\n", module, v->key);
            errno = EINVAL;
            n = -1;
        } else if (is_plain_int(v->text)) {
            n = fprintf(f, "%-28s = %6s      # %s\n", v->key, v->text, v->desc);
        } else {
            n = fprintf(f, "%-28s = \"%s\"      # %s\n", v->key, v->text, v->desc);
        }
        ok = n >= 0;
    }
    return ok;
}

static bool write_ini(const rini_config* ini, const char* path)
{
    return config_write_file(path, write_values, ini);
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
#endif // CONFIG_ENABLE_DEBUGGER

    if (config_save_hook) config_save_hook(&ini, &project);

    /* Saved image paths are kept, without -s, in the file they were loaded
     * from: the project file, or the user file (older configurations, and
     * the one-file setup where -c and -P name the same file). */
    if (!config.arguments.config_save) {
        static const char* const kept[] = { "ROM_FILENAME", "EEPROM_FILENAME", "TF_FILENAME" };
        for (unsigned i = 0; i < sizeof(kept) / sizeof(kept[0]); i++) {
            const char* value = project_ini.values ?
                rini_get_config_value_text_fallback(project_ini, kept[i], NULL) : NULL;
            if (value) {
                rini_set_config_value_text(&project, kept[i], value, "");
                continue;
            }
            value = user_ini.values ? rini_get_config_value_text_fallback(user_ini, kept[i], NULL) : NULL;
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
    if(user_ini.values != NULL) {
        rini_unload_config(&user_ini);
    }
    if(project_ini.values != NULL) {
        rini_unload_config(&project_ini);
    }
}

int config_get(const char *key, int defaultValue) {
    return lookup_int(key, defaultValue);
}
const char* config_get_text(const char *key, const char *defaultValue) {
    return lookup_text(key, defaultValue);
}

void config_set(const char *key, int value, const char *desc) {
    rini_set_config_value(&user_ini, key, value, desc);
}
void config_set_text(const char *key, const char *value, const char *desc) {
    rini_set_config_value_text(&user_ini, key, value, desc);
}
