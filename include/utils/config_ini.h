/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "utils/config.h"
/* Settings hold paths and debugger expressions: values up to 4095 bytes,
 * a full host pathname (PATH_MAX is 4096 with its terminator on Linux,
 * 1024 on macOS), and lines long enough for the key, a quoted value and
 * its description.
 * A longer value is cut when set and config_save() then refuses to write
 * that file (it is left unchanged). Set here, before rini.h, so every C and
 * C++ user sees the same rini_config_value layout: rini.h must only be
 * included through this header. */
#if defined(RINI_H)
#error "include utils/config.h instead of rini.h (rini_config_value layout)"
#endif
#define ZEAL_INI_VALUE_MAX  4095    /* longest value written and read back */
/* One more for the terminator, one more so a value cut when set (exactly
 * RINI_MAX_TEXT_SIZE - 1 bytes) is told apart from the longest valid one */
#define RINI_MAX_TEXT_SIZE  (ZEAL_INI_VALUE_MAX + 2)
/* key (padded) + " = " + quoted value + "      # " + description + EOL */
#define RINI_MAX_LINE_SIZE  (RINI_MAX_TEXT_SIZE + 256)
#include "rini.h"
#ifdef __cplusplus
static_assert(sizeof(rini_config_value) == RINI_MAX_KEY_SIZE + ZEAL_INI_VALUE_MAX + 2 + RINI_MAX_DESC_SIZE, "rini layout");
#else
_Static_assert(sizeof(rini_config_value) == RINI_MAX_KEY_SIZE + ZEAL_INI_VALUE_MAX + 2 + RINI_MAX_DESC_SIZE, "rini layout");
#endif


/**
 * @brief Optional frontend hook, called by config_save() to append
 * frontend-specific keys before the files are written: `user` holds the
 * emulator-wide settings (layout, windows, preferences), `project` what
 * belongs to the program being developed (breakpoints, watches...).
 */
typedef void (*config_save_hook_t)(rini_config* user, rini_config* project);
extern config_save_hook_t config_save_hook;

/**
 * @brief Declare that this frontend manages the project keys starting with
 * `prefix` (e.g. "BREAKPOINT_"): config_save() writes what the frontend
 * provides for them and drops the others. Every other key loaded from the
 * project file is kept as is, so a frontend never erases what another one
 * (or another build) saved there. `prefix` must stay valid.
 */
void config_own_project_keys(const char* prefix);

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

