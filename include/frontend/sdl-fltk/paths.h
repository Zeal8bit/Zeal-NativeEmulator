/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
const char* fltk_executable_dir(void);
const char* get_config_path(void);
/**
 * @brief Non-zero when both paths name the same existing file: relative and
 * absolute spellings, symbolic links and hard links. Uses the file identity
 * (device, inode) when the platform provides one, else (e.g. Windows, where
 * st_ino is 0) compares canonical paths. Paths that do not exist compare
 * by their canonical or literal spelling.
 */
int path_same_file(const char* a, const char* b);
#ifdef __cplusplus
}
#endif
