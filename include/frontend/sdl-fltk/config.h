/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_USER_FILENAME "zeal-fltk.toml"
#define CONFIG_PROJECT_FILENAME "zeal.toml"

typedef struct {
    const char* user_path;
    const char* project_path;
    bool save_arguments;
} fltk_settings_args_t;
extern fltk_settings_args_t fltk_settings_args;

/**
 * @brief Load frontend configuration; return nonzero on failure
 */
int config_parse_file(const char* file);

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

#ifdef __cplusplus
}
#endif
