#pragma once

#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include "rini.h"

typedef struct {
    int width;
    int height;
    int x;
    int y;
    int display;
} config_window_t;

typedef enum {
    DEBUGGER_STATE_ARG_DISABLE = -1,
    DEBUGGER_STATE_DISABLED    = 0,
    DEBUGGER_STATE_CONFIG      = 1,
    DEBUGGER_STATE_ARG         = 2,
} DebuggerState;

typedef struct {
    DebuggerState enabled;
    bool config_enabled;
} config_debugger_t;

typedef struct {
    const char* config_path;
    const char* rom_filename;
    const char* hostfs_path;
    const char* map_file;
    bool config_save;
    bool verbose;
} config_arguments_t;

typedef struct {
    config_debugger_t debugger;
    config_window_t window; // main window options
    config_arguments_t arguments;
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
 * @brief Save config file
 */
int config_save();

/**
 * @brief Update the configs values for window settings
 * @param debug_enabled Whether the user is in debug mode
 */
void config_window_update(bool debug_enabled);

/**
 * @brief Set the Window to the current config settings
 */
void config_window_set(void);
