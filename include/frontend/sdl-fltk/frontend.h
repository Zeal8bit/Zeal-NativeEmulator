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
#include "frontend/sdl-fltk/bridge/machine.h"
#include "frontend/sdl-fltk/snes_adapter.h"
#include "frontend/sdl-fltk/bridge/input.h"

/* The machine driven by this frontend, set by frontend_init. */
extern zeal_t* fltk_machine;

/* Keyboard (keyboard.c) */
void fltk_keyboard_bind(zeal_t* machine);
void fltk_keyboard_unbind(void);
void fltk_keyboard_reset(void);
/** Keys held as the machine sees them (logical zeal_key_t codes). */
int  fltk_keyboard_held(uint16_t* out, int max);
/** Scancodes sent to the machine, newest first. */
int  fltk_keyboard_log(fz_scancode_t* out, int max);
uint32_t fltk_keyboard_log_total(void);

/* Interrupt/raster state captured with each CPU sample (bridge/cpu.c) */
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
/* Identity of the device in slot `index` (SDL joystick instance id), 0 when
 * none: a different value means a different device, even in the same slot */
uint32_t    snes_controller_id(uint8_t index);
const char* snes_controller_name(uint8_t index);
uint16_t    snes_controller_latch(snes_controller_t* ctrl);
/** Live button state (active low, 0xFFFF when unavailable), no side effects. */
uint16_t    snes_controller_bits(uint8_t index);

/* SNES mouse (mouse.c) */
void     snes_mouse_init(snes_mouse_t* mouse);
void     snes_mouse_detach(snes_mouse_t* mouse);
void     snes_mouse_reset_scale(snes_mouse_t* mouse);
void     snes_mouse_update(snes_mouse_t* mouse);
void     snes_mouse_toggle_capture(snes_mouse_t* mouse);
uint32_t snes_mouse_latch(snes_mouse_t* mouse);

/* Render profiling (profile/render_profile.c with -Dprofile_render=true,
 * profile/profile_disabled.c otherwise) */
void   fltk_profile_bind(zvb_t* zvb);
void   fltk_profile_unbind(zvb_t* zvb);
/** Bracket the presentation of one frame. */
double fltk_profile_frame_begin(void);
void   fltk_profile_frame_end(double start);

/* Debugger-only machine helpers (debug/), reached through debug.h hooks */
/* UART console capture (debug/console.c) */
void fltk_console_bind(zeal_t* machine);
/* Live CPU sampling (debug/sampler.c) */
void fltk_sampler_bind(zeal_t* machine);
void fltk_sampler_unbind(void);
/* Statistical PC profiler for the Hot Functions view (debug/pcprof.c) */
void fltk_pcprof_bind(zeal_t* machine);
void fltk_pcprof_unbind(void);
/* VRAM debug rasteriser (debug/vram.c) */
bool fltk_debug_video_render(zvb_t* zvb, dbg_vram_t view, fz_image_t* out);
bool fltk_debug_sprite(zvb_t* zvb, int index, fz_sprite_t* info, uint8_t* rgb, uint8_t* mask);
void fltk_debug_video_free(void);
