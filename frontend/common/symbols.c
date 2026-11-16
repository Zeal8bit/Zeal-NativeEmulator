/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debugger/debugger.h"
#include "frontend/common/symbols.h"
#include "utils/log.h"
#define MAX_LINE_LENGTH 256

bool debugger_load_symbols(dbg_t* dbg, const char* filename)
{
    if (!dbg || !filename) {
        log_perror("[MAP] No debugger or filename");
        return false;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        log_perror("[MAP] Could not open file to load");
        return false;
    }

    symbols_t* current = &dbg->symbols;

    char line[MAX_LINE_LENGTH];
    char name[MAX_LINE_LENGTH];
    unsigned int address;
    char type[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        /* Parse the line and ensure it contains "addr" */
        if (sscanf(line, "%s = $%x ; %s,", name, &address, type) == 3 && strcmp(type, "addr,") == 0 ) {
            /* Store the symbol */
            current->array[current->count].name = strdup(name);
            current->array[current->count].addr = (hwaddr)address;
            current->count++;

            /* If the current array is full, allocate a new symbols_t block */
            if (current->count >= DBG_SYM_COUNT) {
                current->next = (symbols_t*) calloc(1, sizeof(symbols_t));
                if (!current->next) {
                    fclose(file);
                    log_perror("[MAP] Memory allocation failed");
                    return false;  // Memory allocation failed
                }
                current = current->next;
            }
        }
    }

    fclose(file);
    log_printf("[MAP] %s loaded successfully\n", filename);
    return true;
}
