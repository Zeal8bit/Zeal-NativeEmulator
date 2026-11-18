/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Map files loaded into the debugger's symbol table (debug/symbols.c).
 * The map given with -m counts as the first loaded file. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FZ_MAP_MAX 8

/** Number of map files currently loaded. */
int fz_map_count(void);

/** Path of loaded map file `index` (0 = first loaded). */
const char* fz_map_path(int index);

/** Load a map file. `append` keeps the symbols already loaded, otherwise they
 *  are replaced. Nothing changes if the file cannot be read. */
bool fz_map_open(const char* path, bool append);

/** Reload every loaded map file (after rebuilding the program). Returns the
 *  number of files that loaded. */
int fz_map_reload(void);

/** Remove every symbol and forget the loaded files. */
void fz_map_clear(void);

/** Number of symbols in the debugger's table. */
int fz_symbol_count(void);

#ifdef __cplusplus
}
#endif
