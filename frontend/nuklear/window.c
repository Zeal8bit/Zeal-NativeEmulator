/* SPDX-License-Identifier: Apache-2.0 */
#include "raylib.h"
#include "utils/config.h"
#include "hw/zvb/zvb.h"
void config_window_update(bool dbg_enabled) {
#ifdef PLATFORM_WEB
    return;
#endif
    if(dbg_enabled) {
        config.debugger.width = GetScreenWidth();
        config.debugger.height = GetScreenHeight();
        Vector2 position = GetWindowPosition();
        config.debugger.x = position.x;
        config.debugger.y = position.y;
    } else {
        config.window.width = GetScreenWidth();
        config.window.height = GetScreenHeight();
        Vector2 position = GetWindowPosition();
        config.window.x = position.x;
        config.window.y = position.y;
    }
    config.window.display = GetCurrentMonitor();
}

Vector2 config_aspect_force(Vector2 size) {
    Vector2 aspect = { .x = 4, .y = 3 };
#ifdef PLATFORM_WEB
    return aspect;
#endif
    // calculate aspect for missing size

    if(size.x >= size.y) {
        size.y = (size.x * aspect.y) / aspect.x;
    } else {
        size.x = (size.y * aspect.x) / aspect.y;
    }

    return size;
}

bool config_keyboard_passthru(bool dbg_enabled) {
    if(!dbg_enabled) return false; // only enable passthru in debugger ui
    return config.debugger.keyboard_passthru;
}

void config_window_set(bool dbg_enabled) {
#ifdef PLATFORM_WEB
    SetWindowSize(1280, 960);
    printf("PLATFORM_WEB: config_window_set disabled (%d, %d)\n", GetScreenWidth(), GetScreenHeight());
    return;
#endif

    int d = config.window.display >= 0 ? config.window.display : GetCurrentMonitor();
    SetWindowMonitor(d);

    Vector2 window_size = {
        .x = config.window.width,
        .y = config.window.height,
    };

    if(dbg_enabled) {
        window_size.x = config.debugger.width;
        window_size.y = config.debugger.height;
    }

    if(window_size.x < 0 && window_size.y < 0) {
        // default
        if(dbg_enabled) {
            window_size.x = (ZVB_MAX_RES_WIDTH * 2);
            window_size.y = (ZVB_MAX_RES_HEIGHT * 2);
        } else {
            window_size.x = ZVB_MAX_RES_WIDTH;
            window_size.y = ZVB_MAX_RES_HEIGHT;
        }
    }

    if(!dbg_enabled && config.window.aspect_force) {
        window_size = config_aspect_force(window_size);
    }

    SetWindowSize(window_size.x, window_size.y);

    Vector2 screen_offset = GetMonitorPosition(d);
    Vector2 screen = {
        .x = GetMonitorWidth(d),
        .y = GetMonitorHeight(d),
    };

    Vector2 window_pos = {
        .x = config.window.x,
        .y = config.window.y,
    };

    if(dbg_enabled) {
        window_pos.x = config.debugger.x;
        window_pos.y = config.debugger.y;
    }

    // calculate center if either coordinate is not set
    if(window_pos.x < 0) window_pos.x = screen_offset.x + ((screen.x - window_size.x) / 2);
    if(window_pos.y < 0) window_pos.y = screen_offset.y + ((screen.y - window_size.y) / 2);
    SetWindowPosition(window_pos.x, window_pos.y);
}
