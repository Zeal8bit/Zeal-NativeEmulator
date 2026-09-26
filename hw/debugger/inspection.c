/* SPDX-License-Identifier: Apache-2.0 */
#include "debugger/api.h"
#include "hw/zeal.h"
#include <string.h>

dbg_status_t debugger_mappings(dbg_t* dbg, dbg_mapping_t out[4])
{
    if (!dbg || !dbg->arg || !out) return DBG_INVALID;
    zeal_t* m = dbg->arg;
    for (unsigned i=0;i<4;++i) {
        uint32_t page=m->cpu.mmu.pages[i];
        const device_t* dev=m->cpu.mmu.mem_mapping[page].dev;
        out[i]=(dbg_mapping_t){.virtual_address=i*16384,.physical_address=page*16384,.page=page};
        if (dev && dev->name) snprintf(out[i].device,sizeof(out[i].device),"%s",dev->name);
    }
    return DBG_OK;
}
dbg_status_t debugger_counters(dbg_t* dbg, dbg_counter_t out[8])
{
    if (!dbg || !dbg->arg || !out) return DBG_INVALID;
    zeal_t* m=dbg->arg;
    for(unsigned i=0;i<8;++i) {
        const semihost_counter_t* c=&m->semihost.counters[i];
        out[i]=(dbg_counter_t){c->start_cyc,c->last_split_cyc,c->min_us,c->max_us,
            c->total_interval_us,c->last_total_us,c->sample_count,c->running,c->break_on_update};
    }
    return DBG_OK;
}
dbg_status_t debugger_counter_break(dbg_t* dbg, uint32_t index, uint32_t enabled)
{
    if (!dbg || !dbg->arg || enabled>1) return DBG_INVALID;
    if (index>=8) return DBG_RANGE;
    ((zeal_t*)dbg->arg)->semihost.counters[index].break_on_update=enabled;
    return DBG_OK;
}
