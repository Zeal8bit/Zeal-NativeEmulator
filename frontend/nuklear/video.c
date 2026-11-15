/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/nuklear/video.h"
#include "utils/config.h"
#include "utils/log.h"
#include <string.h>

nuklear_video_t nuklear_video;
#if CONFIG_PROFILE_RENDER
typedef struct {
    double window_start;
    double frame_total;
    double frame_max;
    double zvb_total;
    double zvb_max;
    uint64_t frames;
    uint64_t zvb_calls;
} render_profile_t;

static render_profile_t s_render_profile;
static double last_core_seconds;
#endif
#if CONFIG_PROFILE_RENDER
static double profile_time(void* userdata)
{
    (void) userdata;
    return GetTime();
}
static void profile_render(void* userdata, double start)
{
    (void) userdata;
    const double elapsed = GetTime() - start;
    last_core_seconds = elapsed;
    s_render_profile.zvb_total += elapsed;
    if (elapsed > s_render_profile.zvb_max) s_render_profile.zvb_max = elapsed;
    s_render_profile.zvb_calls++;
}
#endif

void nuklear_video_init(zvb_t* zvb)
{
    Image image = {zvb->blitter.framebuffer, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT, 1, PIXELFORMAT_UNCOMPRESSED_R5G6B5};
    nuklear_video.output_texture = LoadTextureFromImage(image);
    nuklear_video.main_texture = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);
#if CONFIG_ENABLE_DEBUGGER
    const int widths[DBG_VIEW_TOTAL] = {ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_WIDTH, 273, 273, 145};
    const int heights[DBG_VIEW_TOTAL] = {ZVB_DBG_RES_HEIGHT, ZVB_DBG_RES_HEIGHT, 545, 273, 209};
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        nuklear_video.debug_img[i] = GenImageColor(widths[i], heights[i], BLANK);
        nuklear_video.debug_tex[i] = LoadTextureFromImage(nuklear_video.debug_img[i]);
    }
#endif
#if CONFIG_PROFILE_RENDER
    zvb->profile_time = profile_time;
    zvb->profile_render = profile_render;
#endif
}

void nuklear_video_upload(zvb_t* zvb)
{
#if CONFIG_PROFILE_RENDER
    const double upload_start = GetTime();
#endif
    zvb_frame_t frame = zvb_frame(zvb);
#if ZVB_BLITTER_SOFTWARE_SCALING
    /* Keep the optional CPU scaling at the presentation boundary. */
    if (frame.width == 320 && frame.height == 240) {
        for (int y = 0; y < frame.height; y++) {
            for (int x = 0; x < frame.width; x++) {
                const uint16_t pixel = frame.pixels[y * 640 + x];
                const int offset = y * 2 * 640 + x * 2;
                nuklear_video.scaled[offset] = pixel;
                nuklear_video.scaled[offset + 1] = pixel;
                nuklear_video.scaled[offset + 640] = pixel;
                nuklear_video.scaled[offset + 641] = pixel;
            }
        }
        frame.pixels = nuklear_video.scaled;
        frame.width = 640;
        frame.height = 480;
    }
#endif
    UpdateTexture(nuklear_video.output_texture, frame.pixels);
    BeginTextureMode(nuklear_video.main_texture);
    DrawTexturePro(nuklear_video.output_texture,
        (Rectangle){0, 0, frame.width, -frame.height},
        (Rectangle){0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT},
        (Vector2){0, 0}, 0, WHITE);
    EndTextureMode();
#if CONFIG_PROFILE_RENDER
    const double upload_seconds = GetTime() - upload_start;
    s_render_profile.zvb_total += upload_seconds;
    if (last_core_seconds + upload_seconds > s_render_profile.zvb_max) {
        s_render_profile.zvb_max = last_core_seconds + upload_seconds;
    }
#endif
}

Texture nuklear_video_output(zvb_t* zvb)
{
    nuklear_video_upload(zvb);
    return nuklear_video.main_texture.texture;
}

void nuklear_video_deinit(void)
{
    if (nuklear_video.output_texture.id) UnloadTexture(nuklear_video.output_texture);
    if (nuklear_video.main_texture.id) UnloadRenderTexture(nuklear_video.main_texture);
#if CONFIG_ENABLE_DEBUGGER
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        if (nuklear_video.debug_tex[i].id) UnloadTexture(nuklear_video.debug_tex[i]);
        if (nuklear_video.debug_img[i].data) UnloadImage(nuklear_video.debug_img[i]);
    }
#endif
    memset(&nuklear_video, 0, sizeof(nuklear_video));
}

#if CONFIG_PROFILE_RENDER
void zvb_profile_frame(double elapsed_seconds)
{
    if (!config.arguments.profile) {
        return;
    }

    const double now = GetTime();
    if (s_render_profile.window_start == 0.0) {
        s_render_profile.window_start = now;
    }
    elapsed_seconds += last_core_seconds;
    s_render_profile.frame_total += elapsed_seconds;
    if (elapsed_seconds > s_render_profile.frame_max) {
        s_render_profile.frame_max = elapsed_seconds;
    }
    s_render_profile.frames++;

    if (now - s_render_profile.window_start < 1.0) {
        return;
    }

    const double frame_avg = s_render_profile.frames > 0
        ? s_render_profile.frame_total / s_render_profile.frames : 0.0;
    const double zvb_avg = s_render_profile.zvb_calls > 0
        ? s_render_profile.zvb_total / s_render_profile.zvb_calls : 0.0;
    log_printf(
        "[PROFILE][ZVB] frames=%llu frame=%.2f/%.2fms zvb=%.2f/%.2fms\n",
        (unsigned long long)s_render_profile.frames,
        frame_avg * 1000.0,
        s_render_profile.frame_max * 1000.0,
        zvb_avg * 1000.0,
        s_render_profile.zvb_max * 1000.0);

    memset(&s_render_profile, 0, sizeof(s_render_profile));
    s_render_profile.window_start = now;
}
#endif
