/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef FRONTEND_SDL_FLTK
#define CONFIG_USER_FILENAME "zeal-fltk.toml"
#define CONFIG_PROJECT_FILENAME "zeal.toml"
#else
#define CONFIG_USER_FILENAME "zeal.ini"
#define CONFIG_PROJECT_FILENAME "zeal.ini"
#endif

typedef enum {
    THEME_SYSTEM,
    THEME_LIGHT,
    THEME_DARK,
} config_theme_t;

typedef struct {
    config_theme_t theme;
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
    int volume;
} config_audio_t;

typedef struct {
    const char* config_path;   /* user settings path */
    const char* project_path;  /* project settings path */
    const char* rom_filename;
    const char* eeprom_filename;
    const char* tf_filename;
    const char* uprog_filename;
    const char* cf_filename;
    const char* hostfs_path;
    const char* map_file;
    const char* breakpoints;
    unsigned long headless_run_ticks;
    bool headless;
    bool console;
    bool config_save;
    uint8_t verbose;
    bool no_reset;
    bool no_dma;
#if ZVB_SCANLINE_RENDERING
    bool no_scanline_rendering;
#endif
#if CONFIG_PROFILE_RENDER
    bool profile;
#endif
} config_arguments_t;

typedef struct {
    config_audio_t audio;
    config_debugger_t debugger;
    config_window_t window; // main window options
    config_arguments_t arguments;
    /* Resolved load paths; keep automatic defaults out of saved arguments. */
    struct {
        const char* rom;
        const char* eeprom;
        const char* tf;
    } images;
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
 * @brief Load frontend configuration; return nonzero on failure
 */
int config_parse_file(const char* file);

/** Resolve image paths after parsing both command-line arguments and the settings file. */
void config_resolve_images(void);

/**
 * @brief Release loaded configuration
 */
void config_unload(void);

/**
 * @brief Save config file
 */
int config_save(void);
/** Atomically replace a configuration file after serialization succeeds. */
bool config_write_file(const char* path, bool (*write_contents)(FILE*, const void*), const void* data);

/**
 * @brief Determine if debugger is enabled
 */
static inline bool config_debugger_enabled(void) {
    // enum uses <= DEBUGGER_STATE_DISABLED for disabled
    return config.debugger.enabled > DEBUGGER_STATE_DISABLED;
}
