/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge_symbols.h"
#include "frontend/common/symbols.h"
#include "debugger/debugger_impl.h"
#include "utils/config.h"

static char* s_paths[FZ_MAP_MAX];
static int s_count;
static bool s_seeded;

/* The -m map was loaded by the common app code before the UI existed */
static void seed(void)
{
    if (s_seeded) return;
    s_seeded = true;
    if (config.arguments.map_file && s_count < FZ_MAP_MAX) {
        s_paths[s_count++] = strdup(config.arguments.map_file);
    }
}

static void forget_all(void)
{
    for (int i = 0; i < s_count; i++) free(s_paths[i]);
    s_count = 0;
}

static bool readable(const char* path)
{
    FILE* f = fopen(path, "r");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

int fz_map_count(void)
{
    seed();
    return s_count;
}

const char* fz_map_path(int index)
{
    seed();
    return index >= 0 && index < s_count ? s_paths[index] : NULL;
}

bool fz_map_open(const char* path, bool append)
{
    seed();
    if (fltk_machine == NULL || path == NULL || !readable(path)) return false;
    dbg_t* dbg = &fltk_machine->dbg;
    if (!append) {
        debugger_clear_symbols(dbg);
        forget_all();
    }
    const bool ok = debugger_load_symbols(dbg, path);
    /* Remember it once, so Reload does not load it twice */
    bool known = false;
    for (int i = 0; i < s_count; i++) known = known || strcmp(s_paths[i], path) == 0;
    if (ok && !known && s_count < FZ_MAP_MAX) s_paths[s_count++] = strdup(path);
    return ok;
}

int fz_map_reload(void)
{
    seed();
    if (fltk_machine == NULL) return 0;
    debugger_clear_symbols(&fltk_machine->dbg);
    int loaded = 0;
    for (int i = 0; i < s_count; i++) {
        if (debugger_load_symbols(&fltk_machine->dbg, s_paths[i])) loaded++;
    }
    return loaded;
}

void fz_map_clear(void)
{
    seed();
    if (fltk_machine) debugger_clear_symbols(&fltk_machine->dbg);
    forget_all();
}

int fz_symbol_count(void)
{
    if (fltk_machine == NULL) return 0;
    int n = 0;
    for (const symbols_t* b = &fltk_machine->dbg.symbols; b; b = b->next) n += (int) b->count;
    return n;
}
