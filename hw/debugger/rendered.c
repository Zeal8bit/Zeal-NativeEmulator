/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zeal.h"
#include <stdlib.h>
#include <string.h>

void debugger_capture_video(dbg_t *dbg)
{
    zeal_t *m = dbg->arg;
    if (m->headless)
        return;
    double started = GetTime();
    Image image = LoadImageFromTexture(m->zvb.blitter.main_texture.texture);
    if (!image.data)
        return;
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    /* The blitters already normalize their output texture for top-down draws. */
    uint32_t bytes = (uint32_t)image.width * (uint32_t)image.height * 4;
    if (bytes > dbg->video_capacity) {
        void *pixels = realloc(dbg->video_pixels, bytes);
        if (!pixels) {
            UnloadImage(image);
            return;
        }
        dbg->video_pixels = pixels;
        dbg->video_capacity = bytes;
    }
    memcpy(dbg->video_pixels, image.data, bytes);
    dbg->video_info =
        (dbg_image_info_t){image.width, image.height, image.width * 4, dbg->video_info.generation + 1};
    UnloadImage(image);
    uint64_t elapsed = (uint64_t)((GetTime() - started) * 1e9);
    dbg->render_stats.frames++;
    dbg->render_stats.total_copy_ns += elapsed;
    if (elapsed > dbg->render_stats.max_copy_ns)
        dbg->render_stats.max_copy_ns = elapsed;
}

dbg_status_t debugger_render_stats(dbg_t *dbg, dbg_render_stats_t *out)
{
    if (!dbg || !out)
        return DBG_INVALID;
    *out = dbg->render_stats;
    return DBG_OK;
}

dbg_status_t debugger_image_copy(dbg_t *dbg, int32_t view, dbg_image_info_t *info, uint8_t *out,
                                 uint32_t capacity)
{
    if (!dbg || !dbg->arg || !info || (capacity && !out))
        return DBG_INVALID;
    if (view < -1 || view >= DBG_VIEW_TOTAL)
        return DBG_RANGE;
    zeal_t *m = dbg->arg;
    if (m->headless)
        return DBG_UNAVAILABLE;
    const uint8_t *pixels;
    if (view == -1) {
        if (!m->dbg_frontend_visible)
            debugger_capture_video(dbg);
        *info = dbg->video_info;
        pixels = dbg->video_pixels;
    } else {
        zvb_render_debug_textures(&m->zvb, (dbg_vram_t)view);
        const Image *image = &m->zvb.debug_img[view];
        *info = (dbg_image_info_t){image->width, image->height, image->width * 4, dbg->video_info.generation};
        pixels = image->data;
    }
    if (!pixels)
        return DBG_UNAVAILABLE;
    uint32_t size = info->stride * info->height;
    if (capacity < size)
        return DBG_CAPACITY;
    memcpy(out, pixels, size);
    return DBG_OK;
}
