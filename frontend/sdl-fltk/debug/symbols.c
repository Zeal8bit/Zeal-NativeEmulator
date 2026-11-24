/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/paths.h"
#include "frontend/sdl-fltk/paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge/symbols.h"
#include "frontend/common/symbols.h"
#include "debugger/debugger_impl.h"
#include "utils/config.h"

/* Desired paths stay in load order, including a saved map that is
 * temporarily unavailable. The first definition of a symbol wins. */
typedef struct map_file {
    struct map_file* next;
    bool loaded;
    char path[];
} map_file_t;

static map_file_t* s_files;
static bool s_seeded;

static map_file_t* new_file(const char* path)
{
    const size_t len = strlen(path) + 1;
    map_file_t* file = malloc(sizeof(*file) + len);
    if (file) {
        file->next = NULL;
        file->loaded = false;
        memcpy(file->path, path, len);
    }
    return file;
}

static void seed(void)
{
    if (s_seeded || fltk_machine == NULL) {
        return;
    }
    s_seeded = true;
    if (config.arguments.map_file) {
        s_files = new_file(config.arguments.map_file);
        /* The common app already attempted the -m map before binding. */
        if (s_files) {
            s_files->loaded = fltk_machine->dbg.symbol_generation != 0;
        }
    }
}

void fz_map_forget(void)
{
    while (s_files) {
        map_file_t* next = s_files->next;
        free(s_files);
        s_files = next;
    }
    s_seeded = false;
}

int fz_map_count(void)
{
    seed();
    int count = 0;
    for (map_file_t* file = s_files; file; file = file->next) count++;
    return count;
}

static const map_file_t* file_at(int index)
{
    seed();
    const map_file_t* file = s_files;
    while (file && index > 0) {
        file = file->next;
        index--;
    }
    return index == 0 ? file : NULL;
}

const char* fz_map_path(int index)
{
    const map_file_t* file = file_at(index);
    return file ? file->path : NULL;
}

bool fz_map_loaded(int index)
{
    const map_file_t* file = file_at(index);
    return file && file->loaded;
}

static bool open_map(const char* path, bool append, bool restoring)
{
    if (fltk_machine == NULL || path == NULL) {
        return false;
    }
    seed();
    map_file_t** end = &s_files;
    int count = 0;
    if (append) {
        while (*end) {
            if (path_same_file(path, (*end)->path)) {
                return (*end)->loaded;
            }
            count++;
            end = &(*end)->next;
        }
        if (count >= FZ_MAP_MAX && !restoring) {
            return false;
        }
    }
    map_file_t* file = new_file(path);
    if (file == NULL) {
        return false;
    }
    file->loaded = count < FZ_MAP_MAX && debugger_load_symbols(&fltk_machine->dbg, path, append);
    if (!file->loaded && !restoring) {
        free(file);
        return false;
    }
    if (!append) {
        fz_map_forget();
        s_seeded = true;
    }
    *end = file;
    return file->loaded;
}

bool fz_map_open(const char* path, bool append)
{
    return open_map(path, append, false);
}

bool fz_map_restore(const char* path)
{
    return open_map(path, true, true);
}

int fz_map_reload(void)
{
    if (fltk_machine == NULL) {
        return 0;
    }
    seed();
    debugger_clear_symbols(&fltk_machine->dbg);
    int loaded = 0;
    for (map_file_t* file = s_files; file; file = file->next) {
        file->loaded = loaded < FZ_MAP_MAX && debugger_load_symbols(&fltk_machine->dbg, file->path, true);
        if (file->loaded) {
            loaded++;
        }
    }
    return loaded;
}

void fz_map_clear(void)
{
    if (fltk_machine) {
        debugger_clear_symbols(&fltk_machine->dbg);
    }
    fz_map_forget();
    s_seeded = true;
}

int fz_symbol_count(void)
{
    if (fltk_machine == NULL) {
        return 0;
    }
    int n = 0;
    for (const symbols_t* b = &fltk_machine->dbg.symbols; b; b = b->next) n += (int) b->count;
    return n;
}
