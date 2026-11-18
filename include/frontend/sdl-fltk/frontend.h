/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private C interfaces of the SDL/FLTK frontend (machine side only). */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hw/zeal.h"
#include "frontend/sdl-fltk/bridge.h"
#include "frontend/sdl-fltk/snes_adapter.h"

/* The machine driven by this frontend, set by frontend_init. */
extern zeal_t* fltk_machine;

/* Keyboard (keyboard.c) */
void fltk_keyboard_bind(zeal_t* machine);
void fltk_keyboard_unbind(void);
void fltk_keyboard_reset(void);

/* Live CPU sampling for the debugger (sampler.c) */
void fltk_sampler_bind(zeal_t* machine);
void fltk_sampler_unbind(void);
/* Interrupt/raster state captured with each sample (bridge_cpu.c) */
void fltk_cpu_sample(zeal_t* machine);
void fltk_cpu_sample_reset(void);

/* Audio (audio.c), SDL3 playback */
void  fltk_audio_init(zvb_sound_t* sound);
void  fltk_audio_deinit(zvb_sound_t* sound);
void  fltk_audio_set_gain(float gain);

/* Gamepads (controller.c), SDL3 */
void        snes_controller_init(snes_controller_t* ctrl, uint8_t index);
void        snes_controller_load_mappings(void);
void        snes_controller_poll(void);
void        snes_controller_shutdown(void);
bool        snes_controller_available(uint8_t index);
const char* snes_controller_name(uint8_t index);
uint16_t    snes_controller_latch(snes_controller_t* ctrl);

/* SNES mouse (mouse.c) */
void     snes_mouse_init(snes_mouse_t* mouse);
void     snes_mouse_detach(snes_mouse_t* mouse);
void     snes_mouse_reset_scale(snes_mouse_t* mouse);
void     snes_mouse_update(snes_mouse_t* mouse);
void     snes_mouse_toggle_capture(snes_mouse_t* mouse);
uint32_t snes_mouse_latch(snes_mouse_t* mouse);

#if CONFIG_ENABLE_DEBUGGER
/* VRAM debug rasteriser (debug_video.c) */
bool fltk_debug_video_render(zvb_t* zvb, dbg_vram_t view, fz_image_t* out);
bool fltk_debug_sprite(zvb_t* zvb, int index, fz_sprite_t* info, uint8_t* rgb, uint8_t* mask);
void fltk_debug_video_free(void);
#endif

#if CONFIG_PROFILE_RENDER
void fltk_profile_bind(zvb_t* zvb);
void fltk_profile_frame(double elapsed_seconds);
double fltk_seconds(void);
#endif
