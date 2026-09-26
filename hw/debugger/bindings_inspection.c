/* SPDX-License-Identifier: Apache-2.0 */
#include "bindings_internal.h"
#include <string.h>
DBG_EXPORT uint32_t zeal_debug_mappings(uint32_t h, uint8_t *out, uint32_t capacity)
{
    dbg_mapping_t maps[4];
    dbg_status_t s = debugger_mappings(debugger_resolve(h), maps);
    if (s != DBG_OK)
        return s;
    if (capacity < 304)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    for (unsigned i = 0; i < 4; i++) {
        uint8_t *p = out + i * 76;
        dbg_put32(p, maps[i].virtual_address);
        dbg_put32(p + 4, maps[i].physical_address);
        dbg_put32(p + 8, maps[i].page);
        memcpy(p + 12, maps[i].device, 64);
    }
    return DBG_OK;
}
DBG_EXPORT uint32_t zeal_debug_counters(uint32_t h, uint8_t *out, uint32_t capacity)
{
    dbg_counter_t counters[8];
    dbg_status_t s = debugger_counters(debugger_resolve(h), counters);
    if (s != DBG_OK)
        return s;
    if (capacity < 480)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    for (unsigned i = 0; i < 8; i++) {
        uint8_t *p = out + i * 60;
        dbg_counter_t *c = &counters[i];
        dbg_put64(p, c->start);
        dbg_put64(p + 8, c->last_split);
        dbg_put64(p + 16, c->minimum_us);
        dbg_put64(p + 24, c->maximum_us);
        dbg_put64(p + 32, c->total_us);
        dbg_put64(p + 40, c->last_us);
        dbg_put32(p + 48, c->samples);
        dbg_put32(p + 52, c->running);
        dbg_put32(p + 56, c->break_on_update);
    }
    return DBG_OK;
}
DBG_EXPORT uint32_t zeal_debug_counter_break(uint32_t h, uint32_t index, uint32_t enabled)
{
    return debugger_counter_break(debugger_resolve(h), index, enabled);
}
DBG_EXPORT uint32_t zeal_debug_image(uint32_t h, int32_t view, uint8_t *info, uint32_t info_capacity,
                                     uint8_t *pixels, uint32_t capacity)
{
    if (info_capacity < 20)
        return DBG_CAPACITY;
    if (!info)
        return DBG_INVALID;
    dbg_image_info_t result = {0};
    dbg_status_t s = debugger_image_copy(debugger_resolve(h), view, &result, pixels, capacity);
    if (s != DBG_OK && s != DBG_CAPACITY)
        return s;
    dbg_put32(info, result.width);
    dbg_put32(info + 4, result.height);
    dbg_put32(info + 8, result.stride);
    dbg_put64(info + 12, result.generation);
    return s;
}

DBG_EXPORT uint32_t zeal_debug_vram(uint32_t h, int32_t view, uint32_t index, uint8_t *out, uint32_t capacity)
{
    dbg_vram_info_t v;
    dbg_status_t status = debugger_vram_info(debugger_resolve(h), view, index, &v);
    if (status != DBG_OK)
        return status;
    if (capacity < 44)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    uint32_t values[] = {v.mode, v.graphics, v.bitmap, v.cell_width, v.cell_height, v.columns,
                         v.rows, v.index,    v.value,  v.attributes, v.palette_rgb};
    for (unsigned i = 0; i < 11; i++)
        dbg_put32(out + 4 * i, values[i]);
    return DBG_OK;
}
