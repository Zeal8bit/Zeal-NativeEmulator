/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* SNES mouse driven by the host pointer over the FLTK screen widget. The
 * widget accumulates motion between latches; a middle click captures the
 * pointer (hidden and confined) so motion is unlimited. */

#include <math.h>
#include <stdio.h>
#include "frontend/sdl-fltk/frontend.h"
#include "utils/notif.h"

static uint8_t snes_mouse_magnitude(float value)
{
    int magnitude = (int) roundf(fabsf(value));
    if (magnitude > SNES_MOUSE_MAG_MASK) magnitude = SNES_MOUSE_MAG_MASK;
    return (uint8_t) magnitude;
}

static bool snes_mouse_active(const snes_mouse_t* mouse)
{
    return mouse->attached && (mouse->captured || mouse->hovered);
}

static void snes_mouse_capture(snes_mouse_t* mouse)
{
    mouse->captured = true;
    mouse->delta_x = mouse->delta_y = 0.0f;
    ui_pointer_lock(true);
    printf("[SNES] Mouse captured\n");
    notif_show("Mouse captured (middle click to release)");
}

static void snes_mouse_release(snes_mouse_t* mouse)
{
    mouse->captured = false;
    ui_pointer_lock(false);
    printf("[SNES] Mouse released\n");
}

void snes_mouse_init(snes_mouse_t* mouse)
{
    mouse->attached = true;
    mouse->captured = false;
    mouse->capture_button_down = false;
    mouse->machine = NULL;
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
    mouse->hovered = false;
    mouse->button_left = mouse->button_right = false;
    mouse->delta_x = mouse->delta_y = 0.0f;
}

void snes_mouse_detach(snes_mouse_t* mouse)
{
    mouse->attached = false;
    mouse->capture_button_down = false;
    if (mouse->captured) snes_mouse_release(mouse);
}

void snes_mouse_reset_scale(snes_mouse_t* mouse)
{
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
    notif_show("SNES Mouse Scale: x%.2f", mouse->delta_scale);
}

void snes_mouse_toggle_capture(snes_mouse_t* mouse)
{
    if (mouse->captured) {
        snes_mouse_release(mouse);
    } else if (mouse->attached && mouse->hovered) {
        snes_mouse_capture(mouse);
    }
}

void snes_mouse_update(snes_mouse_t* mouse)
{
    if (!mouse->attached && mouse->captured) snes_mouse_release(mouse);
}

uint32_t snes_mouse_latch(snes_mouse_t* mouse)
{
    const bool active = snes_mouse_active(mouse);
    float dx = 0.0f;
    float dy = 0.0f;

    if (active) {
        /* Window pixels -> emulated pixels -> mickeys */
        float sx = 1.0f;
        float sy = 1.0f;
        ui_screen_scale(&sx, &sy);
        dx = mouse->delta_x * sx * mouse->delta_scale;
        dy = mouse->delta_y * sy * mouse->delta_scale;
        /* Keep what does not fit in one packet for the next latch */
        const float max = SNES_MOUSE_MAG_MASK;
        const float cx = dx > max ? max : dx < -max ? -max : dx;
        const float cy = dy > max ? max : dy < -max ? -max : dy;
        const float unscale_x = sx * mouse->delta_scale;
        const float unscale_y = sy * mouse->delta_scale;
        mouse->delta_x = unscale_x > 0.0f ? (dx - cx) / unscale_x : 0.0f;
        mouse->delta_y = unscale_y > 0.0f ? (dy - cy) / unscale_y : 0.0f;
        dx = cx;
        dy = cy;
    } else {
        mouse->delta_x = mouse->delta_y = 0.0f;
    }

    return snes_mouse_packet(snes_mouse_magnitude(dx), dx < 0.0f,
        snes_mouse_magnitude(dy), dy < 0.0f, SNES_MOUSE_DEFAULT_SPEED,
        active && mouse->button_left, active && mouse->button_right);
}

/* ------------------------------------------------------------------ */
/*  Bridge: events from the screen widget                              */
/* ------------------------------------------------------------------ */

void fz_mouse_motion(float dx, float dy)
{
    snes_mouse_t* mouse = &frontend_snes.mouse;
    if (!snes_mouse_active(mouse)) return;
    mouse->delta_x += dx;
    mouse->delta_y += dy;
}

void fz_mouse_buttons(bool left, bool right)
{
    frontend_snes.mouse.button_left = left;
    frontend_snes.mouse.button_right = right;
}

void fz_mouse_hover(bool inside)
{
    frontend_snes.mouse.hovered = inside;
}

void fz_mouse_wheel(float steps)
{
    snes_mouse_t* mouse = &frontend_snes.mouse;
    if (!snes_mouse_active(mouse) || steps == 0.0f) return;
    mouse->delta_scale += steps * SNES_MOUSE_DELTA_SCALE_STEP;
    if (mouse->delta_scale < SNES_MOUSE_DELTA_SCALE_MIN) {
        mouse->delta_scale = SNES_MOUSE_DELTA_SCALE_MIN;
    } else if (mouse->delta_scale > SNES_MOUSE_DELTA_SCALE_MAX) {
        mouse->delta_scale = SNES_MOUSE_DELTA_SCALE_MAX;
    }
    notif_show("SNES Mouse Speed: x%.2f", mouse->delta_scale);
}

void fz_mouse_capture_toggle(void)
{
    snes_mouse_toggle_capture(&frontend_snes.mouse);
}

bool fz_mouse_captured(void)
{
    return frontend_snes.mouse.captured;
}
