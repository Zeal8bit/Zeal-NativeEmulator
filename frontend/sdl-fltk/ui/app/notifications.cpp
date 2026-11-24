/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/notifications.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include "app/main_window.h"
#include "app/screen_view.h"
#include "common/machine.h"

namespace zui::app {

constexpr double NOTIF_LIFETIME = 1.5;
static char s_notif_text[96];
static bool s_notif_active;

static void notif_expire(void*)
{
    s_notif_active = false;
    if (MainWindow* window = main_window()) {
        window->notify("");
        window->screen()->redraw();
    }
}

const char* notif_current_text()
{
    return s_notif_active ? s_notif_text : nullptr;
}

void notif_cancel()
{
    Fl::remove_timeout(notif_expire);
}

} // namespace zui::app

using namespace zui::app;

extern "C" {

void notif_reset(void)
{
    s_notif_active = false;
    s_notif_text[0] = '\0';
}

void notif_show(const char* fmt, ...)
{
    if (fmt == nullptr) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_notif_text, sizeof(s_notif_text), fmt, args);
    va_end(args);
    s_notif_active = true;
    if (MainWindow* window = main_window()) {
        window->notify(s_notif_text);
        window->screen()->redraw();
    }
    Fl::remove_timeout(notif_expire);
    Fl::add_timeout(NOTIF_LIFETIME, notif_expire);
}

bool notif_visible(void)
{
    return s_notif_active;
}

int notif_estimate_width(void)
{
    return s_notif_active ? (int) (strlen(s_notif_text) * 10) : 0;
}

void notif_render(int x, int y)
{
    (void) x;
    (void) y;
}

} // extern "C"
