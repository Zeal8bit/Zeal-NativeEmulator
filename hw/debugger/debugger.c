#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "debugger/debugger.h"
#include "utils/log.h"

#define MAX_LINE_LENGTH     256

bool debugger_is_breakpoint_set(dbg_t *dbg, hwaddr address)
{
    /* Consider that 0 is invalid */
    if (dbg == NULL || address == 0) {
        return false;
    }
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->breakpoints[i] == address) {
            return true;
        }
    }
    return false;
}

bool debugger_set_breakpoint(dbg_t *dbg, hwaddr address)
{
    /* Forbid breakpoint at 0*/
    if (dbg == NULL || address == 0 || debugger_is_breakpoint_set(dbg, address)) {
        return false;
    }
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->breakpoints[i] == 0) {
            dbg->breakpoints[i] = address;
            return true;
        }
    }
    return false;
}

bool debugger_clear_breakpoint(dbg_t *dbg, hwaddr address)
{
    if (dbg == NULL || address == 0) {
        return false;
    }
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->breakpoints[i] == address) {
            dbg->breakpoints[i] = 0;
            return true;
        }
    }
    return false;
}

bool debugger_toggle_breakpoint(dbg_t *dbg, hwaddr address) {
    if(debugger_is_breakpoint_set(dbg, address)) {
        return debugger_clear_breakpoint(dbg, address);
    } else {
        return debugger_set_breakpoint(dbg, address);
    }
}


int debugger_get_breakpoints(dbg_t *dbg, hwaddr *bp, unsigned int size)
{
    if (dbg == NULL || bp == NULL) {
        return 0;
    }
    unsigned int found = 0;
    for (int i = 0; i < DBG_MAX_POINTS && found < size; i++) {
        if (dbg->breakpoints[i] != 0) {
            bp[found++] = dbg->breakpoints[i];
        }
    }
    return found;
}


/* Watchpoint management */
bool debugger_add_watchpoint(dbg_t *dbg, watchpoint_t wp) {
    if (!dbg) {
        return false;
    }
    /* Check if a watchpoint already exists at the address */
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->watchpoints[i].addr == wp.addr) {
            dbg->watchpoints[i].type |= wp.type;
            return true;
        }
    }
    /* Find an empty slot and add the new watchpoint */
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->watchpoints[i].addr == 0) {
            dbg->watchpoints[i] = wp;
            return true;
        }
    }
    return false;
}


bool debugger_remove_watchpoint(dbg_t *dbg, hwaddr address) {
    if (dbg == NULL) {
        return false;
    }
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->watchpoints[i].addr == address) {
            dbg->watchpoints[i].addr = 0;
            dbg->watchpoints[i].type = 0;
            return true;
        }
    }
    return false;
}

bool debugger_is_watchpoint_set(dbg_t *dbg, hwaddr address) {
    if (dbg == NULL) {
        return false;
    }
    for (int i = 0; i < DBG_MAX_POINTS; i++) {
        if (dbg->watchpoints[i].addr == address) {
            return true;
        }
    }
    return false;
}

int debugger_get_watchpoints(dbg_t *dbg, watchpoint_t *wps, unsigned int size) {
    if (dbg == NULL || wps == NULL || size == 0) {
        return 0;
    }

    unsigned int found = 0;
    for (int i = 0; i < DBG_MAX_POINTS && found < size; i++) {
        if (dbg->watchpoints[i].addr != 0) {
            wps[found++] = dbg->watchpoints[i];
        }
    }

    return found;
}


/* Memory inspection */
void debugger_read_memory(dbg_t *dbg, hwaddr addr, int len, uint8_t *val)
{
    if (dbg == NULL || val == NULL || len == 0) {
        return;
    }
    dbg->get_mem_cb(dbg, addr, len, val);
}

void debugger_write_memory(dbg_t *dbg, hwaddr addr, int len, uint8_t *val)
{
    if (dbg == NULL || val == NULL || len == 0) {
        return;
    }
    dbg->set_mem_cb(dbg, addr, len, val);
}


/* Register access */
void debugger_get_registers(dbg_t *dbg, regs_t *regs)
{
    if (dbg == NULL || regs == NULL) {
        return;
    }
    dbg->get_regs_cb(dbg, regs);
}

void debugger_set_registers(dbg_t *dbg, regs_t *regs)
{
    if (dbg == NULL || regs == NULL) {
        return;
    }
    dbg->set_regs_cb(dbg, regs);
}


/* Execution control */
void debugger_step(dbg_t *dbg)
{
    if (dbg == NULL || dbg->step_cb == NULL) {
        return;
    }
    dbg->step_cb(dbg);
}

/* Execution control */
void debugger_step_over(dbg_t *dbg)
{
    if (dbg == NULL || dbg->step_over_cb == NULL) {
        return;
    }
    // dbg->step_cb(dbg);
    log_printf("Step Over - not implemented\n");
}

void debugger_breakpoint(dbg_t *dbg) {
    if (dbg == NULL || dbg->breakpoint_cb == NULL) {
        return;
    }
    dbg->breakpoint_cb(dbg);
}

void debugger_continue(dbg_t *dbg)
{
    if (dbg == NULL || dbg->continue_cb == NULL) {
        return;
    }
    dbg->continue_cb(dbg);
}


void debugger_pause(dbg_t *dbg)
{
    if (dbg == NULL || dbg->pause_cb == NULL) {
        return;
    }
    dbg->pause_cb(dbg);
}

void debugger_reset(dbg_t *dbg)
{
    if (dbg == NULL || dbg->reset_cb == NULL) {
        return;
    }
    dbg->reset_cb(dbg);
}


bool debugger_is_paused(dbg_t *dbg)
{
    if (dbg == NULL || dbg->is_paused_cb == NULL) {
        return false;
    }
    return dbg->is_paused_cb(dbg);
}


/* Event handling */
debug_event_t debugger_check_event(dbg_t *dbg)
{
    (void) dbg;
    return DEBUG_EVENT_NONE;
}


void debugger_handle_event(dbg_t *dbg, debug_event_t event)
{
    (void) dbg;
    (void) event;
}


/* Symbol management */
bool debugger_load_symbols(dbg_t *dbg, const char *filename) {
    if (!dbg || !filename) {
        log_perror("[MAP] No debugger or filename");
        return false;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        log_perror("[MAP] Could not open file to load");
        return false;
    }

    symbols_t *current = &dbg->symbols;

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
                current->next = (symbols_t *)calloc(1, sizeof(symbols_t));
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


const char* debugger_get_symbol(dbg_t *dbg, hwaddr address)
{
    if (dbg == NULL) {
        return NULL;
    }
    /* FIXME */
    if (address == 0) {
        return "";
    }

    symbols_t *current = &dbg->symbols;

    while (current) {
        for (unsigned int i = 0; i < current->count; i++) {
            if (current->array[i].addr == address) {
                return current->array[i].name;
            }
        }
        current = current->next;
    }

    return NULL;
}

bool debugger_find_symbol(dbg_t *dbg, const char *symbol_name, hwaddr *address)
{
    if (dbg == NULL || symbol_name == NULL || address == NULL) {
        return false;
    }

    symbols_t *current = &dbg->symbols;

    while (current) {
        for (unsigned int i = 0; i < current->count; i++) {
            if (strcmp(current->array[i].name, symbol_name) == 0) {
                *address = current->array[i].addr;
                return true;
            }
        }
        current = current->next;
    }

    return false;
}

/* Disassembly */
int debugger_disassemble(dbg_t *dbg, hwaddr address, char *buffer, int size)
{
    if (dbg == NULL) {
        return 0;
    }
    (void) address;
    (void) buffer;
    (void) size;
    return 0;
}


int debugger_disassemble_address(dbg_t *dbg, hwaddr address, dbg_instr_t* instr)
{
    if (dbg == NULL || dbg->disassemble_cb == NULL || instr == NULL) {
        return -1;
    }

    int bytes = dbg->disassemble_cb(dbg, address, instr);

    /* Check if the current address has a label */
    const char* label = debugger_get_symbol(dbg, address);
    if (label != NULL) {
        strncpy(instr->label, label, sizeof(instr->label));
    } else {
        /* Make sure to return an empty label */
        instr->label[0] = 0;
    }

    return bytes;
}


/* Debugger initialization */
void debugger_init(dbg_t *dbg)
{
    if (dbg == NULL) {
        return;
    }
    memset(dbg, 0, sizeof(dbg_t));
}


void debugger_deinit(dbg_t *dbg)
{
    if (dbg == NULL) {
        return;
    }
    (void) dbg;
}
