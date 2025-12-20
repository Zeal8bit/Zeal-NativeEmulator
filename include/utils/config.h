/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include "rini.h"
#include "raylib.h"


typedef struct {
    int width;
    int height;
    int x;
    int y;
    int display;
    bool aspect_force;
} config_window_t;

typedef enum {
    DEBUGGER_STATE_ARG_DISABLE = -1,
    DEBUGGER_STATE_DISABLED    = 0,
    DEBUGGER_STATE_CONFIG      = 1,
    DEBUGGER_STATE_ARG         = 2,
    DEBUGGER_STATE_USER        = 3,
} debugger_state_t;

typedef struct {
    debugger_state_t enabled;
    bool config_enabled;

    bool keyboard_passthru; // whether to pass all keypresses through to emulator
    bool hex_upper;

    int width;
    int height;
    int x;
    int y;
} config_debugger_t;

typedef struct {
    const char* config_path;
    const char* rom_filename;
    const char* eeprom_filename;
    const char* tf_filename;
    const char* uprog_filename;
    const char* cf_filename;
    const char* hostfs_path;
    const char* map_file;
    const char* breakpoints;
    bool headless;
    bool config_save;
    bool verbose;
    bool no_reset;
} config_arguments_t;

typedef struct {
    config_debugger_t debugger;
    config_window_t window; // main window options
    config_arguments_t arguments;
    rini_config ini;
} config_t;

extern config_t config;

/**
 * @brief Usage message for CLI
 */
int usage(const char *progname);
void config_debug(void);

/**
 * @brief Parse command args
 */
int parse_command_args(int argc, char* argv[]);

/**
 * @brief Parse the INI Config File
 */
void config_parse_file(const char* file);

/**
 * @brief Free/unload the rini_config
 */
void config_unload(void);

/**
 * @brief Save config file
 */
int config_save(void);

/**
 * @brief Get a config int value
 * @param key The key to retrieve
 * @param defaultValue Default value if key not set
 */
int config_get(const char *key, int defaultValue);
/**
 * @brief Get a config string value
 * @param key The key to retrieve
 * @param defaultValue Default value if key not set
 */
const char* config_get_text(const char *key, const char *defaultValue);

/**
 * @brief Set a config int value
 * @param key The key to set
 * @param value The int value
 * @param desc The description of the key
 */
void config_set(const char *key, int value, const char *desc);

/**
 * @brief Set a config string value
 * @param key The key to set
 * @param value The string value
 * @param desc The description of the key
 */
void config_set_text(const char *key, const char *value, const char *desc);

/**
 * @brief Update the configs values for window settings
 * @param dbg_enabled Whether the user is in debug mode
 */
void config_window_update(bool dbg_enabled);

/**
 * @brief Set the Window to the current config settings
 */
void config_window_set(bool dbg_enabled);

/**
 * @brief Force aspect ratio on Window Size
 * @param size The initial size
 * @return The size forced into the aspect ratio (larger dim remain)
 */
Vector2 config_aspect_force(Vector2 size);

Vector2 config_get_next_resolution(int width);
Vector2 config_get_prev_resolution(int width);

bool config_keyboard_passthru(bool dbg_enabled);

/**
 * @brief Determine if debugger is enabled
 */
static inline bool config_debugger_enabled(void) {
    // enum uses <= DEBUGGER_STATE_DISABLED for disabled
    return config.debugger.enabled > DEBUGGER_STATE_DISABLED;
}
