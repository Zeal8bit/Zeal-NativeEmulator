/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zeal.h"
#include "host/zeal_host.h"
#include <stdlib.h>
#include <string.h>

void debugger_capture_video(dbg_t *dbg)
{
    zeal_t *m = dbg->arg;
    if (m->headless)
        return;
    double started = zeal_host_time();
    int width = 0, height = 0, pitch = 0;
    bool rgb565 = false;
    const void *source = zvb_output_pixels(&m->zvb, &width, &height, &pitch, &rgb565);
    if (source == NULL) {
#if ZVB_BLITTER_SHADER
        /* Only a GPU blitter needs the frame read back from its texture. */
        Image image = LoadImageFromTexture(m->zvb.blitter.main_texture.texture);
        if (!image.data)
            return;
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        width = image.width;
        height = image.height;
        rgb565 = false;
        uint32_t bytes = (uint32_t)width * (uint32_t)height * 4;
        if (bytes > dbg->video_capacity) {
            void *grown = realloc(dbg->video_pixels, bytes);
            if (!grown) {
                UnloadImage(image);
                return;
            }
            dbg->video_pixels = grown;
            dbg->video_capacity = bytes;
        }
        memcpy(dbg->video_pixels, image.data, bytes);
        UnloadImage(image);
        source = dbg->video_pixels;
#else
        return;
#endif
    }
    uint32_t bytes = (uint32_t)width * (uint32_t)height * 4;
    if (bytes > dbg->video_capacity) {
        void *grown = realloc(dbg->video_pixels, bytes);
        if (!grown)
            return;
        dbg->video_pixels = grown;
        dbg->video_capacity = bytes;
    }
    uint8_t *out = dbg->video_pixels;
    if (rgb565) {
        /* Expand RGB565 to the 32-bit pixels the debugger front-ends expect. */
        const uint16_t *src = source;
        for (uint32_t i = 0; i < (uint32_t)width * height; i++) {
            uint16_t p = src[i];
            out[i * 4 + 0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            out[i * 4 + 1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
            out[i * 4 + 2] = (uint8_t)((p & 0x1F) * 255 / 31);
            out[i * 4 + 3] = 255;
        }
    } else if (source != out) {
        memcpy(out, source, bytes);
    }
    dbg->video_info =
        (dbg_image_info_t){width, height, width * 4, dbg->video_info.generation + 1};
    uint64_t elapsed = (uint64_t)((zeal_host_time() - started) * 1e9);
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
        const zvb_debug_image_t *image = &m->zvb.debug_img[view];
        *info = (dbg_image_info_t){image->width, image->height, image->width * 4,
                                   dbg->video_info.generation};
        pixels = (const uint8_t*)image->pixels;
    }
    if (!pixels)
        return DBG_UNAVAILABLE;
    uint32_t size = info->stride * info->height;
    if (capacity < size)
        return DBG_CAPACITY;
    memcpy(out, pixels, size);
    return DBG_OK;
}
