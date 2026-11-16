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
} wasm_video_t;
extern wasm_video_t wasm_video;
void wasm_video_init(zvb_t* zvb);
void                wasm_video_deinit(void);
Texture wasm_video_output(zvb_t* zvb);
void wasm_video_upload(zvb_t* zvb);
#if CONFIG_PROFILE_RENDER
void zvb_profile_frame(double elapsed_seconds);
#endif
