/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "raylib.h"
#include "hw/zvb/zvb.h"

typedef struct {
#if ZVB_BLITTER_SOFTWARE_SCALING
    uint16_t scaled[640 * 480];
#endif
    Texture2D output_texture;
    RenderTexture main_texture;
#if CONFIG_ENABLE_DEBUGGER
    Image debug_img[DBG_VIEW_TOTAL];
    Texture debug_tex[DBG_VIEW_TOTAL];
#endif
} nuklear_video_t;
extern nuklear_video_t nuklear_video;
void nuklear_video_init(zvb_t* zvb);
void nuklear_video_deinit(void);
Texture nuklear_video_output(zvb_t* zvb);
void nuklear_video_upload(zvb_t* zvb);
#if CONFIG_ENABLE_DEBUGGER
void zvb_render_debug_textures(zvb_t* zvb, dbg_vram_t view);
static inline const Texture* zvb_get_debug_textures(zvb_t* zvb, int* count) {
    (void)zvb;
    if (count) *count = DBG_VIEW_TOTAL;
    return nuklear_video.debug_tex;
}
#endif
#if CONFIG_PROFILE_RENDER
void zvb_profile_frame(double elapsed_seconds);
#endif
