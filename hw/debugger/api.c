/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/debugger/debugger_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void debugger_record(dbg_t* dbg, dbg_reason_t reason, uint32_t address, uint32_t access)
{
    if (!dbg) return;
    dbg->reason = reason;
    uint64_t sequence = ++dbg->sequence;
    dbg->events[(sequence - 1) % 64] = (dbg_event_record_t){sequence, reason, address, access};
}

dbg_status_t debugger_command(dbg_t* dbg, dbg_command_t cmd)
{
    if (!dbg || cmd > DBG_STOP || cmd < DBG_PAUSE) return DBG_INVALID;
    if (dbg->stopped) return DBG_UNAVAILABLE;
    debugger_ctrl_op callbacks[] = {dbg->pause_cb, dbg->continue_cb, dbg->step_cb,
        dbg->step_over_cb, dbg->reset_cb, dbg->stop_cb};
    if (!callbacks[cmd]) return DBG_UNAVAILABLE;
    callbacks[cmd](dbg);
    if (cmd == DBG_STOP) { dbg->stopped = true; dbg->running = false; }
    debugger_record(dbg, cmd == DBG_PAUSE ? DBG_REASON_PAUSE : cmd == DBG_STOP ? DBG_REASON_STOP :
        cmd == DBG_RESET ? DBG_REASON_RESET : DBG_REASON_NONE, 0, 0);
    return DBG_OK;
}

dbg_status_t debugger_snapshot(dbg_t* dbg, dbg_snapshot_t* out)
{
    if (!dbg || !out) return DBG_INVALID;
    regs_t regs = {0};
    if (dbg->get_regs_cb) dbg->get_regs_cb(dbg, &regs);
    *out = (dbg_snapshot_t){debugger_is_paused(dbg), dbg->stopped, dbg->reason, regs.pc, dbg->sequence};
    return DBG_OK;
}

dbg_status_t debugger_events(dbg_t* dbg, uint64_t after, dbg_event_record_t* out,
    uint32_t capacity, uint32_t* count, uint32_t* overflow)
{
    if (!dbg || !count || !overflow || (capacity && !out) || after > dbg->sequence) return DBG_INVALID;
    uint64_t first = dbg->sequence > 64 ? dbg->sequence - 64 : 0;
    *overflow = after < first;
    if (after < first) after = first;
    uint32_t available = (uint32_t)(dbg->sequence - after);
    *count = capacity ? (available < capacity ? available : capacity) : available;
    if (!capacity) return available ? DBG_CAPACITY : DBG_OK;
    for (uint32_t i = 0; i < *count; ++i) out[i] = dbg->events[(after + i) % 64];
    return available > capacity ? DBG_CAPACITY : DBG_OK;
}

static dbg_status_t memory(dbg_t* dbg, dbg_address_space_t space, uint32_t addr,
    uint8_t* data, uint32_t size, bool write)
{
    if (!dbg || (size && !data) || (space != DBG_VIRTUAL && space != DBG_PHYSICAL)) return DBG_INVALID;
    uint32_t limit = space == DBG_VIRTUAL ? 65536 : 4194304;
    if (addr >= limit || size > limit - addr) return DBG_RANGE;
    debugger_mem_op op = write ? dbg->set_mem_cb : dbg->get_mem_cb;
    if (!op) return DBG_UNAVAILABLE;
    if (!size) return DBG_OK;
    return op(dbg, addr | (space == DBG_PHYSICAL ? 0x80000000u : 0), (int)size, data) == 0 ? DBG_OK : DBG_UNAVAILABLE;
}
dbg_status_t debugger_memory_read(dbg_t* d, dbg_address_space_t s, uint32_t a, uint8_t* p, uint32_t n)
{ return memory(d,s,a,p,n,false); }
dbg_status_t debugger_memory_write(dbg_t* d, dbg_address_space_t s, uint32_t a, const uint8_t* p, uint32_t n)
{ return memory(d,s,a,(uint8_t*)p,n,true); }

dbg_status_t debugger_symbols_text(dbg_t* dbg, const char* text, uint32_t length)
{
    if (!dbg || (!text && length)) return DBG_INVALID;
    uint32_t start = 0;
    while (start < length) {
        uint32_t end = start;
        while (end < length && text[end] != '\n') ++end;
        if (end - start > 1023) return DBG_RANGE;
        char line[1024], name[256], type[256]; unsigned address;
        memcpy(line,text+start,end-start); line[end-start]=0;
        if (sscanf(line,"%255s = $%x ; %255s",name,&address,type)==3 && strcmp(type,"addr,")==0) {
            symbols_t* b = &dbg->symbols;
            while (b->next) b=b->next;
            if (b->count == DBG_SYM_COUNT) {
                b->next=calloc(1,sizeof(*b)); if (!b->next) return DBG_UNAVAILABLE; b=b->next;
            }
            char* copy = malloc(strlen(name)+1); if (!copy) return DBG_UNAVAILABLE;
            strcpy(copy,name); b->array[b->count++] = (symbol_t){copy,address};
        }
        start=end+1;
    }
    return DBG_OK;
}
