/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "video.h"
#include "utils/notif.h"

#define NOTIF_TEXT_MAX 64
/* How long the notification is displayed for */
#define NOTIF_LIFETIME_SEC 1.5
/* How long the notification takes to fade out */
#define NOTIF_FADE_SEC 0.4

typedef struct {
    bool active;
    /* Absolute time when the current notification was shown */
    double shown_at;
    char text[NOTIF_TEXT_MAX];
} notif_state_t;

static notif_state_t g_notif;

static uint8_t notif_alpha(double elapsed)
{
    if (elapsed >= NOTIF_LIFETIME_SEC) {
        return 0;
    }

    if (elapsed <= (NOTIF_LIFETIME_SEC - NOTIF_FADE_SEC)) {
        return 255;
    }

    /* Fading started! */
    const double fade_left = NOTIF_LIFETIME_SEC - elapsed;
    const double ratio = fade_left / NOTIF_FADE_SEC;
    return (uint8_t) (255.0 * ratio);
}

void notif_reset(void)
{
    memset(&g_notif, 0, sizeof(g_notif));
}

bool notif_visible(void)
{
    if (!g_notif.active) {
        return false;
    }

    if (((SDL_GetTicks() / 1000.0) - g_notif.shown_at) >= NOTIF_LIFETIME_SEC) {
        g_notif.active = false;
        return false;
    }

    return true;
}

void notif_show(const char* fmt, ...)
{
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(g_notif.text, NOTIF_TEXT_MAX, fmt, args);
    va_end(args);

    g_notif.active = true;
    g_notif.shown_at = (SDL_GetTicks() / 1000.0);
}

int notif_estimate_width(void)
{
    if (g_notif.text[0] == '\0') {
        return 0;
    }

    return (int)strlen(g_notif.text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
}

void notif_render(int x, int y)
{
    const double elapsed = (SDL_GetTicks() / 1000.0) - g_notif.shown_at;
    const unsigned char alpha = notif_alpha(elapsed);

    if (!notif_visible() || alpha == 0) {
        g_notif.active = false;
        return;
    }

    SDL_SetRenderDrawColor(wasm_renderer, 255, 227, 69, alpha);
    SDL_RenderDebugText(wasm_renderer, x, y, g_notif.text);
}
