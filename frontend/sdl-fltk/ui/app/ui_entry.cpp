/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The C side's view of the UI: ui_* (bridge/machine.h) and the
 * config_window_* hooks of utils/config.h. */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/platform.H>
#include "app/app_menus.h"
#include "app/debugger_ui.h"
#include "app/main_window.h"
#include "app/notifications.h"
#include "app/screen_view.h"
#include "common/clock.h"
#include "common/machine.h"
#include "common/persist.h"
#include "theme/fonts.h"
#include "theme/theme.h"

using namespace zui::app;
using namespace zui::common;
using namespace zui::theme;

namespace {

/* Developer aid: ZEAL_FLTK_SCREENSHOT=<file.ppm> saves the main window after
 * ZEAL_FLTK_SCREENSHOT_DELAY seconds (default 3), to check the UI headlessly. */
void maybe_screenshot()
{
#if FL_API_VERSION >= 10400
    static bool done;
    const char* path = getenv("ZEAL_FLTK_SCREENSHOT");
    MainWindow* window = main_window();
    if (done || path == nullptr || window == nullptr) return;
    const char* delay = getenv("ZEAL_FLTK_SCREENSHOT_DELAY");
    if (now_seconds() < (delay ? atof(delay) : 3.0)) return;
    done = true;
    Fl_Image_Surface surface(window->w(), window->h(), 1);
    Fl_Surface_Device::push_current(&surface);
    surface.draw(window);
    Fl_RGB_Image* image = surface.image();
    Fl_Surface_Device::pop_current();
    if (image && image->d() >= 3) {
        FILE* f = fopen(path, "wb");
        if (f) {
            const uchar* data = (const uchar*) image->data()[0];
            /* data_w/data_h: pixel size, larger than w/h on high-DPI screens */
            const int pw = image->data_w();
            const int ph = image->data_h();
            const int line = image->ld() ? image->ld() : pw * image->d();
            fprintf(f, "P6 %d %d 255\n", pw, ph);
            for (int y = 0; y < ph; y++) {
                for (int x = 0; x < pw; x++) fwrite(data + y * line + x * image->d(), 1, 3, f);
            }
            fclose(f);
            fprintf(stderr, "[UI] Screenshot saved to %s\n", path);
        }
    }
    delete image;
#endif
}

} // namespace

extern "C" {

void config_window_update(bool dbg_enabled)
{
    if (main_window()) main_window()->capture_window_config(dbg_enabled);
    if (main_window() && main_window()->debugger()) main_window()->debugger()->capture_layout();
}

void config_window_set(bool dbg_enabled)
{
    if (main_window()) main_window()->apply_window_config(dbg_enabled);
}

bool config_keyboard_passthru(bool dbg_enabled)
{
    return dbg_enabled && config.debugger.keyboard_passthru;
}

int ui_open(bool debug)
{
    /* Cocoa (macOS system menubar) must exist before any menu is built */
    fl_open_display();
    apply_ui_font();
    start_theme();
    load_fonts();
#if FL_API_VERSION >= 10400
    Fl::use_high_res_GL(1);
#endif
    Fl::visible_focus(0);

    MainWindow::reset_quit();
    MainWindow* window = new MainWindow(debug);
    window->show();
    window->screen()->show();
    window->screen()->take_focus();
    Fl::check();
    return 0;
}

void ui_close(void)
{
    stop_theme();
    notif_cancel();
    MainWindow* window = main_window();
    if (window == nullptr) return;
    /* Settings owned by the widgets are taken now: the configuration is
     * saved after they are deleted */
    persist_finalize();
    window->screen()->lock_pointer(false);
    window->hide();
    delete window;
    Fl::check();
}

bool ui_closed(void)
{
    return MainWindow::quit_requested() || main_window() == nullptr;
}

void ui_wait(double seconds)
{
    if (main_window() == nullptr) return;
    maybe_screenshot();
    main_window()->poll_menus();
    if (now_seconds() - main_window()->status_at() > 0.25) main_window()->update_status();
    Fl::wait(seconds);
}

void ui_present(const uint16_t* pixels, int width, int height)
{
    if (main_window() == nullptr) return;
    main_window()->screen()->present(pixels, width, height);
    main_window()->frame_presented();
}

void ui_debug_mode(bool enabled)
{
    if (main_window()) main_window()->set_debug_layout(enabled);
}

void ui_machine_bound(void)
{
    if (main_window() && main_window()->debugger()) main_window()->debugger()->machine_bound();
}

void ui_debug_refresh(bool force)
{
    if (main_window() && main_window()->debugger() && main_window()->debug_layout()) {
        main_window()->debugger()->refresh(force);
        if (force) main_window()->update_status();
    }
}

bool ui_screen_focused(void)
{
    return main_window() && Fl::focus() == main_window()->screen();
}

void ui_screen_scale(float* sx, float* sy)
{
    float x = 1.0f;
    float y = 1.0f;
    if (main_window()) {
        int px, py, pw, ph;
        main_window()->screen()->picture_rect(px, py, pw, ph);
        if (pw > 0) x = (float) FZ_SCREEN_WIDTH / pw;
        if (ph > 0) y = (float) FZ_SCREEN_HEIGHT / ph;
    }
    if (sx) *sx = x;
    if (sy) *sy = y;
}

void ui_pointer_lock(bool locked)
{
    if (main_window()) main_window()->screen()->lock_pointer(locked);
}

void ui_gamepads_changed(void)
{
    AppMenus::mark_snes_dirty();
}

} // extern "C"
