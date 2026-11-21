/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    THEME_SYSTEM,
    THEME_LIGHT,
    THEME_DARK,
} config_theme_t;

typedef struct {
    config_theme_t theme;
    int width;
    int height;
    int x;
    int y;
    int display;
    bool aspect_force;
} config_window_t;

typedef struct {
    bool keyboard_passthru; // whether to pass all keypresses through to emulator
    bool hex_upper;

    int width;
    int height;
    int x;
    int y;
} config_debugger_t;

typedef struct {
    int volume;
} config_audio_t;

typedef struct {
    const char* rom_filename;
    const char* eeprom_filename;
    const char* tf_filename;
    const char* uprog_filename;
    const char* cf_filename;
    const char* hostfs_path;
    const char* map_file;
    const char* breakpoints;
    uint8_t verbose;
    bool no_reset;
    bool no_dma;
#if ZVB_SCANLINE_RENDERING
    bool no_scanline_rendering;
#endif
#if CONFIG_PROFILE_RENDER
    bool profile;
#endif
} config_arguments_t;

typedef struct {
    config_audio_t audio;
    config_debugger_t debugger;
    config_window_t window; // main window options
    config_arguments_t arguments;
    /* Resolved load paths; keep automatic defaults out of saved arguments. */
    struct {
        const char* rom;
        const char* eeprom;
        const char* tf;
    } images;
} config_t;

extern config_t config;

void config_debug(void);
void config_resolve_images(void);
