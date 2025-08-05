/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
#include "utils/config.h"
#include "debugger/debugger_ui.h"
#include "hw/zeal.h"
#include "utils/log.h"

typedef struct {
    bool pressed; // TODO: support key repeat with GetTime()??
    bool shifted;
    const char *label;
    KeyboardKey key;
    debugger_callback_t callback;
} debugger_key_t;


static void main_scale_up(dbg_t *dbg) {
#ifdef PLATFORM_WEB
    return;
#endif
    (void)dbg; // unreferenced
    int width = GetScreenWidth();
    Vector2 size = config_get_next_resolution(width);
    SetWindowSize(size.x, size.y);
}

static void main_scale_down(dbg_t *dbg) {
#ifdef PLATFORM_WEB
    return;
#endif
    (void)dbg; // unreferenced
    int width = GetScreenWidth();
    Vector2 size = config_get_prev_resolution(width);
    SetWindowSize(size.x, size.y);
}

static void main_reset(dbg_t *dbg) {
    if (dbg == NULL || dbg->reset_cb == NULL) {
        return;
    }
    dbg->reset_cb(dbg);
}

static debugger_key_t debugger_key_toggle = {
    .label = "Toggle Debugger",
    .key = KEY_F1,
    .callback = zeal_debug_toggle,
    .pressed = false,
    .shifted = false
};

static debugger_key_t main_keys[] = {
    { .label = "Scale Up", .key = KEY_EQUAL, .callback = main_scale_up, .pressed = false, .shifted = true },
    { .label = "Scale Down", .key = KEY_MINUS, .callback = main_scale_down, .pressed = false, .shifted = true },
    { .label = "Reset", .key = KEY_BACKSPACE, .callback = main_reset, .pressed = false, .shifted = true },
};

static debugger_key_t debugger_keys[] = {
    // { .label = "Toggle Debugger", .key = KEY_F1, .callback = zeal_debug_toggle, .pressed = false, .shifted = false },
    { .label = "Pause", .key = KEY_F6, .callback = debugger_pause, .pressed = false, .shifted = false },
    { .label = "Continue", .key = KEY_F5, .callback = debugger_continue, .pressed = false, .shifted = false },
    { .label = "Step Over", .key = KEY_F10, .callback = debugger_step_over, .pressed = false, .shifted = false },
    { .label = "Step", .key = KEY_F11, .callback = debugger_step, .pressed = false, .shifted = false },
    { .label = "Toggle Breakpoint", .key = KEY_F9, .callback = debugger_breakpoint, .pressed = false, .shifted = false },
    { .label = "Reset", .key = KEY_BACKSPACE, .callback = debugger_reset, .pressed = false, .shifted = true },
    { .label = "Scale Up", .key = KEY_EQUAL, .callback = debugger_scale_up, .pressed = false, .shifted = true },
    { .label = "Scale Down", .key = KEY_MINUS, .callback = debugger_scale_down, .pressed = false, .shifted = true },
};

bool zeal_ui_input(zeal_t* machine)
{
    bool handled = false;

    if(config_keyboard_passthru(machine->dbg_enabled)) {
        return handled;
    }

    // Debugger UI requires Ctrl + {KEY}
#ifdef __APPLE__
    // MacOS has too many default bindings for Ctrl + F* keys
    bool meta = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
#else
    bool meta = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
#endif

    if(!meta) {
        return handled; /// all zeal ui keystrokes require meta?
    }

    bool shift = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));

    // toggle between main view and debugger view
    debugger_key_t *opt = &debugger_key_toggle;
    bool pressed = IsKeyDown(opt->key);
    handled = handled || (pressed);
    if(meta && !opt->pressed && pressed) {
        zeal_debug_toggle(&machine->dbg);
        opt->pressed = true;
    } else if(!pressed) {
        opt->pressed = false;
    }

    int keys_size;
    debugger_key_t *keys;
    if(machine->dbg_enabled) {
        keys = debugger_keys;
        keys_size = DIM(debugger_keys);
    } else {
        keys = main_keys;
        keys_size = DIM(main_keys);
    }

    // debugger ui
    for(int i = 0; i < keys_size; i++) {
        opt = &keys[i];
        bool shifted = (opt->shifted == shift);
        pressed = IsKeyDown(opt->key);
        handled = handled || (pressed && shifted);
        if(shifted && !opt->pressed && pressed) {
            opt->callback(&machine->dbg);
            opt->pressed = true;
        } else if(!pressed) {
            opt->pressed = false;
        }
    };

    return handled;
}
