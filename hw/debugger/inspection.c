/* SPDX-License-Identifier: Apache-2.0 */
#include "debugger/api.h"
#include "hw/zeal.h"
#include <string.h>

dbg_status_t debugger_mappings(dbg_t *dbg, dbg_mapping_t out[4])
{
    if (!dbg || !dbg->arg || !out)
        return DBG_INVALID;
    zeal_t *m = dbg->arg;
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t page = m->cpu.mmu.pages[i];
        const device_t *dev = m->cpu.mmu.mem_mapping[page].dev;
        out[i] =
            (dbg_mapping_t){.virtual_address = i * 16384, .physical_address = page * 16384, .page = page};
        if (dev && dev->name)
            snprintf(out[i].device, sizeof(out[i].device), "%s", dev->name);
    }
    return DBG_OK;
}
dbg_status_t debugger_counters(dbg_t *dbg, dbg_counter_t out[8])
{
    if (!dbg || !dbg->arg || !out)
        return DBG_INVALID;
    zeal_t *m = dbg->arg;
    for (unsigned i = 0; i < 8; ++i) {
        const semihost_counter_t *c = &m->semihost.counters[i];
        out[i] = (dbg_counter_t){c->start_cyc,    c->last_split_cyc,    c->min_us,
                                 c->max_us,       c->total_interval_us, c->last_total_us,
                                 c->sample_count, c->running,           c->break_on_update};
    }
    return DBG_OK;
}
dbg_status_t debugger_counter_break(dbg_t *dbg, uint32_t index, uint32_t enabled)
{
    if (!dbg || !dbg->arg || enabled > 1)
        return DBG_INVALID;
    if (index >= 8)
        return DBG_RANGE;
    ((zeal_t *)dbg->arg)->semihost.counters[index].break_on_update = enabled;
    return DBG_OK;
}

dbg_status_t debugger_vram_info(dbg_t *dbg, int32_t view, uint32_t index, dbg_vram_info_t *out)
{
    if (!dbg || !dbg->arg || !out)
        return DBG_INVALID;
    if (view < 0 || view >= DBG_VIEW_TOTAL)
        return DBG_RANGE;
    const zvb_t *v = &((zeal_t *)dbg->arg)->zvb;
    bool gfx = zvb_is_gfx_mode(v), four = v->mode == MODE_GFX_640_4BIT || v->mode == MODE_GFX_320_4BIT;
    bool layer = view == DBG_TILEMAP_LAYER0 || view == DBG_TILEMAP_LAYER1;
    uint32_t cols = layer ? 80 : 16, rows = layer ? 40 : view == DBG_TILESET ? 32 : 16;
    if (index >= cols * rows)
        return DBG_RANGE;
    *out = (dbg_vram_info_t){.mode = v->mode,
                             .graphics = gfx,
                             .bitmap = zvb_is_bitmap_mode(v),
                             .cell_width = layer              ? (gfx ? 17 : 9)
                                           : view == DBG_FONT ? 9
                                                              : 17,
                             .cell_height = layer              ? (gfx ? 17 : 13)
                                            : view == DBG_FONT ? 13
                                                               : 17,
                             .columns = cols,
                             .rows = rows,
                             .index = index,
                             .value = index};
    if (layer) {
        out->value = view == DBG_TILEMAP_LAYER0 ? v->layers.raw_layer0[index] : v->layers.raw_layer1[index];
        out->attributes = v->layers.raw_layer1[index];
        if (gfx && four)
            out->value = v->layers.raw_layer0[index] + ((out->attributes & 1) ? 256 : 0);
    }
    if (view == DBG_PALETTE) {
        const uint8_t *p = v->palette.raw_palette + index * 2;
        uint16_t rgb = p[0] | ((uint16_t)p[1] << 8);
        out->value = rgb;
        out->palette_rgb = ((((rgb >> 11) & 31) * 255 / 31) << 16) | ((((rgb >> 5) & 63) * 255 / 63) << 8) |
                           ((rgb & 31) * 255 / 31);
    }
    return DBG_OK;
}
