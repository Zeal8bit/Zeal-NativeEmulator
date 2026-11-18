/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "debugger/debugger.h"
#include "debugger/debugger_impl.h"
#include "frontend/common/symbols.h"
#include "utils/log.h"
#define MAX_LINE_LENGTH 256

/* Strip leading and trailing blanks in place */
static char* trim(char* s)
{
    while (isspace((unsigned char) *s)) s++;
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char) end[-1])) *--end = '\0';
    return s;
}

/* SDCC table values are zero-padded hex ("000094F5") */
static bool is_hex_address(const char* s)
{
    size_t n = strlen(s);
    if (n < 4) return false;
    for (size_t i = 0; i < n; i++) {
        if (!isxdigit((unsigned char) s[i])) return false;
    }
    return true;
}

static bool contains_nocase(const char* s, const char* what)
{
    const size_t n = strlen(what);
    for (; *s; s++) {
        if (strncasecmp(s, what, n) == 0) return true;
    }
    return false;
}

/* Kind of symbol from its section/area name. SDCC: _CODE, _TEXT, _HOME,
 * _GSINIT, _DATA, _BSS, _INITIALIZED, _RODATA...; z88dk: code_user,
 * data_user, bss_compiler, rodata_user, TEXT, DATA, BSS... */
static uint8_t section_kind(const char* section)
{
    if (section == NULL || section[0] == '\0') return SYM_KIND_UNKNOWN;
    if (contains_nocase(section, "rodata") || contains_nocase(section, "const") ||
        contains_nocase(section, "initializer")) {
        return SYM_KIND_CONST;
    }
    if (contains_nocase(section, "data") || contains_nocase(section, "bss") ||
        contains_nocase(section, "initialized") || contains_nocase(section, "heap")) {
        return SYM_KIND_DATA;
    }
    if (contains_nocase(section, "code") || contains_nocase(section, "text") ||
        contains_nocase(section, "home") || contains_nocase(section, "gsinit") ||
        contains_nocase(section, "gsfinal") || contains_nocase(section, "system") ||
        contains_nocase(section, "header")) {
        return SYM_KIND_CODE;
    }
    return SYM_KIND_UNKNOWN;
}

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

    unsigned int total = 0;

    /* Append after any symbols already loaded (several map files) */
    symbols_t* current = &dbg->symbols;
    while (current->next) current = current->next;
    if (current->count >= DBG_SYM_COUNT) {
        current->next = (symbols_t*) calloc(1, sizeof(symbols_t));
        if (!current->next) {
            fclose(file);
            log_perror("[MAP] Memory allocation failed");
            return false;
        }
        current = current->next;
    }

    char line[MAX_LINE_LENGTH];
    char name[MAX_LINE_LENGTH];
    char value[MAX_LINE_LENGTH];
    char area[MAX_LINE_LENGTH] = "";
    unsigned int address;
    /* SDCC (ASxxxx linker) maps list globals in one "Value  Global" table
     * per area; the .ABS. area only holds section sizes/starts (l__, s__) */
    bool sdcc_table = false;

    while (fgets(line, sizeof(line), file)) {
        bool found = false;
        uint8_t kind = SYM_KIND_UNKNOWN;
        bool local = false;

        if (strstr(line, "Value  Global")) {
            sdcc_table = true;
            continue;
        }
        if (strncmp(line, "ASxxxx Linker", 13) == 0 || strncmp(line, "Area ", 5) == 0 ||
            strncmp(line, "Files Linked", 12) == 0) {
            sdcc_table = false;
            continue;
        }
        if (strstr(line, " bytes (")) {
            /* Area line: "_CODE   00004000   00000020 =   32. bytes (REL,CON)" */
            if (strstr(line, ".ABS.")) strcpy(area, ".ABS.");
            else if (sscanf(line, "%255s", area) != 1) area[0] = '\0';
            continue;
        }
        if (sdcc_table) {
            /* "     000094F5  _sound_init     sounds" */
            if (strcmp(area, ".ABS.") != 0 && sscanf(line, " %255s %255s", value, name) == 2 &&
                is_hex_address(value)) {
                address = (unsigned int) strtoul(value, NULL, 16);
                kind = section_kind(area);
                found = true;
            }
        } else if (sscanf(line, "%255s = $%x ; %255[^\n]", name, &address, value) == 3) {
            /* z88dk: "name = $addr ; addr, public, , module, SECTION, file:line" */
            char* fields[6] = { 0 };
            int n = 0;
            for (char* p = value; n < 6; n++) {
                fields[n] = p;
                char* comma = strchr(p, ',');
                if (!comma) { n++; break; }
                *comma = '\0';
                p = comma + 1;
            }
            if (n >= 1 && strcmp(trim(fields[0]), "addr") == 0) {
                local = n >= 2 && strcmp(trim(fields[1]), "local") == 0;
                kind = n >= 5 ? section_kind(trim(fields[4])) : SYM_KIND_UNKNOWN;
                found = true;
            }
        }
        if (found) {
            /* Store the symbol */
            symbol_t* sym = &current->array[current->count];
            sym->name = strdup(name);
            sym->addr = (hwaddr)address;
            sym->kind = kind;
            sym->local = local;
            current->count++;
            total++;

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
    log_printf("[MAP] %s loaded successfully (%u symbols)\n", filename, total);
    return true;
}
