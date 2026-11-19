/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Main window: menu bar, the screen or the debugger layout, toolbar and
 * status bar, window geometry and screen scaling. */

#include "app/main_window.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <initializer_list>
#include <FL/Fl_Sys_Menu_Bar.H>
#include "app/app_menus.h"
#include "app/debugger_ui.h"
#include "app/screen_view.h"
#include "common/clock.h"
#include "common/format.h"
#include "common/machine.h"
#include "common/persist.h"
#include "theme/theme.h"
#include "widgets/dock_layout.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>

namespace zui::app {

using namespace zui::common;
using namespace zui::theme;
using namespace zui::widgets;

namespace {

MainWindow* s_window;
bool s_closed;

constexpr int TOOLBAR_H = 34;
constexpr int STATUS_H = 22;

float scale_quantize_tenths(float scale, float step)
{
    const float scaled_tenths = scale * 10.0f;
    const float rounded_tenths = std::round(scaled_tenths);
    float target;
    if (std::fabs(scaled_tenths - rounded_tenths) < 0.001f) {
        target = rounded_tenths + step;
    } else {
        target = step > 0.0f ? std::ceil(scaled_tenths) : std::floor(scaled_tenths);
    }
    if (target < 1.0f) target = 1.0f;
    return target / 10.0f;
}

void window_cb(Fl_Widget*, void*)
{
    /* Escape belongs to the emulated machine, never closes the window */
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;
    MainWindow::quit();
}

} // namespace

MainWindow* main_window()
{
    return s_window;
}

void MainWindow::quit()
{
    s_closed = true;
    fz_quit();
}

bool MainWindow::quit_requested()
{
    return s_closed;
}

void MainWindow::reset_quit()
{
    s_closed = false;
}

/* ------------------------------------------------------------------ */
/*  MainWindow                                                         */
/* ------------------------------------------------------------------ */

MainWindow::MainWindow(bool debug)
    : Fl_Double_Window(FZ_SCREEN_WIDTH, FZ_SCREEN_HEIGHT, FZ_WIN_NAME)
{
#ifdef CONFIG_SHOW_FPS
    show_fps = CONFIG_SHOW_FPS;
#endif
    /* Debugger screen scale, in percent (0 = fit the panel) */
    m_debug_zoom = preference("debugger", "screen_zoom", 0) / 100.0f;
    persist_register([this]() {
        set_preference("debugger", "screen_zoom", (int) std::lround(m_debug_zoom * 100.0f));
    });
    s_window = this;
    color(COLOR_BG);
    callback(window_cb);

#ifdef __APPLE__
    m_menubar_h = 0;
#else
    m_menubar_h = 26;
#endif
    m_menubar = new Fl_Sys_Menu_Bar(0, 0, w(), m_menubar_h);
    m_menubar->box(FL_FLAT_BOX);

    m_dock = new DockLayout(0, m_menubar_h, w(), h() - m_menubar_h - STATUS_H);
    m_dock->end();

    m_statusbar = new Fl_Group(0, h() - STATUS_H, w(), STATUS_H);
    m_statusbar->box(FL_FLAT_BOX);
    m_statusbar->color(COLOR_PANEL);
    m_status_state = new Fl_Box(6, h() - STATUS_H, 200, STATUS_H);
    m_status_msg = new Fl_Box(206, h() - STATUS_H, w() - 206 - 220, STATUS_H);
    m_status_info = new Fl_Box(w() - 220, h() - STATUS_H, 214, STATUS_H);
    for (Fl_Box* b : { m_status_state, m_status_msg, m_status_info }) {
        b->labelsize(12);
        b->labelcolor(COLOR_DIM);
        b->box(FL_NO_BOX);
    }
    m_status_state->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_status_msg->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    m_status_msg->labelcolor(COLOR_LABEL);
    m_status_info->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    m_statusbar->resizable(m_status_msg);
    m_statusbar->end();

    /* The screen and the debugger docks live inside the dock layout */
    m_dock->begin();
    m_screen = new ScreenView(0, m_menubar_h, w(), h() - m_menubar_h - STATUS_H);
    m_screen->end();
    m_dock->end();
    m_debugger = create_debugger_ui(m_dock, m_screen);
    if (m_debugger) {
        begin();
        m_toolbar = m_debugger->toolbar(0, m_menubar_h, w(), TOOLBAR_H);
        end();
        m_toolbar->hide();
    } else {
        m_dock->set_root(m_screen, nullptr);
    }
    end();

    resizable(m_dock);
    apply_size_range();
    m_menus = new AppMenus(m_menubar, this);

    m_debug = !debug; /* force the initial layout */
    set_debug_layout(debug);
}

MainWindow::~MainWindow()
{
    delete m_debugger;
    delete m_menus;
    if (s_window == this) s_window = nullptr;
}

int MainWindow::chrome_height() const
{
    return m_menubar_h + (m_debug ? TOOLBAR_H : 0) + STATUS_H;
}

void MainWindow::layout_chrome()
{
    const int tb = m_debug ? TOOLBAR_H : 0;
    m_menubar->resize(0, 0, w(), m_menubar_h);
    if (m_toolbar) {
        m_toolbar->resize(0, m_menubar_h, w(), TOOLBAR_H);
        if (m_debug) m_toolbar->show(); else m_toolbar->hide();
    }
    m_dock->resize(0, m_menubar_h + tb, w(), h() - m_menubar_h - tb - STATUS_H);
    m_statusbar->resize(0, h() - STATUS_H, w(), STATUS_H);
    layout_status();
    init_sizes();
    redraw();
}

void MainWindow::apply_size_range()
{
    int min_w = 320;
    int min_h = 240;
    if (m_debug && m_debugger) {
        /* The debugger panels need more room; never more than the screen */
        m_debugger->min_body_size(min_w, min_h);
        int sx, sy, sw, sh;
        Fl::screen_work_area(sx, sy, sw, sh, shown() ? screen_num() : 0);
        if (min_w > sw) min_w = sw;
        if (min_h + chrome_height() > sh) min_h = sh - chrome_height();
    }
    size_range(min_w, min_h + chrome_height());
    if (w() < min_w || h() < min_h + chrome_height()) {
        resize(x(), y(), std::max(w(), min_w), std::max(h(), min_h + chrome_height()));
    }
}

bool MainWindow::aspect_locked() const
{
    return !m_debug && config.window.aspect_force && !fullscreen_active();
}

void MainWindow::resize(int nx, int ny, int nw, int nh)
{
    /* Force 4:3: the picture area (window minus menu and status bars)
     * follows the edge that changed most */
    if (aspect_locked() && shown() && (nw != w() || nh != h())) {
        const int chrome = chrome_height();
        if (std::abs(nw - w()) >= std::abs(nh - h())) {
            nh = nw * 3 / 4 + chrome;
        } else {
            nw = (nh - chrome) * 4 / 3;
        }
    }
    Fl_Double_Window::resize(nx, ny, nw, nh);
    if (m_statusbar) layout_status();
}

void MainWindow::layout_status()
{
    /* State | message | info, the sides capped to a third each, so that no
     * part gets a negative width in a narrow window */
    const int y0 = m_statusbar->y();
    const int avail = std::max(0, w() - 12);
    const int state_w = std::min(200, avail / 3);
    const int info_w = std::min(214, avail / 3);
    const int msg_w = std::max(0, avail - state_w - info_w);
    m_status_state->resize(6, y0, state_w, STATUS_H);
    m_status_msg->resize(6 + state_w, y0, msg_w, STATUS_H);
    m_status_info->resize(6 + state_w + msg_w, y0, info_w, STATUS_H);
}

void MainWindow::apply_aspect()
{
    if (!aspect_locked()) return;
    Fl_Double_Window::resize(x(), y(), w(), w() * 3 / 4 + chrome_height());
}

int MainWindow::handle(int event)
{
    if (event == FL_UNFOCUS || event == FL_HIDE) {
        fz_keys_release_all();
        fz_mouse_release_all();
    }
    return Fl_Double_Window::handle(event);
}

void MainWindow::set_debug_layout(bool debug)
{
    if (debug && m_debugger == nullptr) debug = false;
    if (debug == m_debug) return;
    m_debug = debug;
    /* The normal window is sized to the picture, so it always fits */
    m_screen->zoom(debug ? m_debug_zoom : 0.0f);
    label(debug ? FZ_WIN_NAME " - Debugger" : FZ_WIN_NAME);
    m_dock->docks_visible(debug);
    layout_chrome();
    apply_size_range();
    apply_window_config(debug);
    update_menus();
    update_status();
    if (m_screen->shown()) m_screen->take_focus();
}

void MainWindow::apply_window_config(bool debug)
{
    int W, H;
    int X = debug ? config.debugger.x : config.window.x;
    int Y = debug ? config.debugger.y : config.window.y;

    if (debug) {
        W = config.debugger.width;
        H = config.debugger.height;
        if (W <= 0 || H <= 0) {
            m_debugger->default_body_size(W, H);
            H += chrome_height();
        }
    } else {
        int sw = config.window.width;
        int sh = config.window.height;
        if (sw <= 0 || sh <= 0) {
            sw = FZ_SCREEN_WIDTH;
            sh = FZ_SCREEN_HEIGHT;
        }
        if (config.window.aspect_force) {
            if (sw >= sh) sh = sw * 3 / 4; else sw = sh * 4 / 3;
        }
        W = sw;
        H = sh + chrome_height();
    }

    int sx, sy, sw, sh;
    const int display = config.window.display >= 0 && config.window.display < Fl::screen_count()
        ? config.window.display : 0;
    Fl::screen_work_area(sx, sy, sw, sh, display);
    if (W > sw) W = sw;
    if (H > sh) H = sh;
    if (X < 0 || X > sx + sw - 40) X = sx + (sw - W) / 2;
    if (Y < 0 || Y > sy + sh - 40) Y = sy + (sh - H) / 2;
    resize(X, Y, W, H);

    if (debug && m_debugger) m_debugger->apply_layout();
}

void MainWindow::capture_window_config(bool debug)
{
    if (fullscreen_active()) return;
    if (debug) {
        if (m_debugger) m_debugger->capture_layout();
        config.debugger.x = x();
        config.debugger.y = y();
        config.debugger.width = w();
        config.debugger.height = h();
    } else {
        config.window.x = x();
        config.window.y = y();
        config.window.width = m_screen->w();
        config.window.height = m_screen->h();
    }
    config.window.display = Fl::screen_num(x(), y());
}

void MainWindow::set_screen_scale(float scale)
{
    if (fullscreen_active()) return;
    if (m_debug) {
        /* Debugger: scale the picture inside its panel, never the window
         * (that would reshuffle every panel). Capped at what fits. */
        const float fit = m_screen->fit_scale();
        if (scale < 0.5f) scale = 0.5f;
        if (scale >= fit - 0.01f) {
            m_debug_zoom = 0.0f;
            notif_show("Scale: x%.2f (fits the panel)", fit);
        } else {
            m_debug_zoom = scale;
            notif_show("Scale: x%.1f", scale);
        }
        m_screen->zoom(m_debug_zoom);
        return;
    }
    const int target_w = (int) std::lround(FZ_SCREEN_WIDTH * scale);
    const int target_h = (int) std::lround(FZ_SCREEN_HEIGHT * scale);
    resize(x(), y(), w() + target_w - m_screen->w(), h() + target_h - m_screen->h());
    notif_show("Scale: x%.1f", scale);
}

void MainWindow::scale_screen(float step)
{
    int px, py, pw, ph;
    m_screen->picture_rect(px, py, pw, ph);
    set_screen_scale(scale_quantize_tenths((float) pw / FZ_SCREEN_WIDTH, step));
}

void MainWindow::toggle_fullscreen()
{
    if (fullscreen_active()) fullscreen_off(); else fullscreen();
}

void MainWindow::reset_layout()
{
    if (m_debugger) m_debugger->reset_layout();
    if (m_debug) {
        config.debugger.width = config.debugger.height = -1;
        config.debugger.x = config.debugger.y = -1;
        apply_window_config(true);
    }
    update_menus();
}

void MainWindow::update_menus()
{
    m_menus->update();
}

void MainWindow::poll_menus()
{
    if (AppMenus::snes_dirty()) m_menus->rebuild_snes();
}

void MainWindow::notify(const char* text)
{
    m_status_msg->copy_label(text);
    m_statusbar->redraw();
}

void MainWindow::frame_presented()
{
    m_fps_frames++;
    const double t = now_seconds();
    if (t - m_fps_epoch >= 1.0) {
        m_fps = (int) (m_fps_frames / (t - m_fps_epoch) + 0.5);
        m_fps_frames = 0;
        m_fps_epoch = t;
        update_status();
    }
}

void MainWindow::update_status()
{
    char state[96];
    snprintf(state, sizeof(state), fz_debugger_enabled() ? "Running (debugger)" : "Running");
    unsigned pc_value;
    if (m_debugger && m_debugger->paused_at(&pc_value)) {
        char pc[8];
        snprintf(pc, sizeof(pc), hex16_fmt(), pc_value);
        char reason[48];
        if (m_debugger->pause_reason(reason, sizeof(reason))) {
            snprintf(state, sizeof(state), "Paused at $%s: %s", pc, reason);
        } else {
            snprintf(state, sizeof(state), "Paused at $%s", pc);
        }
    }
    m_status_state->copy_label(state);

    char info[96];
    int px, py, pw, ph;
    m_screen->picture_rect(px, py, pw, ph);
    const float scale = (float) pw / FZ_SCREEN_WIDTH;
    int n = snprintf(info, sizeof(info), "x%.2f", scale);
    if (fz_muted()) n += snprintf(info + n, sizeof(info) - n, "   Muted");
    else n += snprintf(info + n, sizeof(info) - n, "   Vol %d%%", fz_volume());
    if (show_fps) snprintf(info + n, sizeof(info) - n, "   %d fps", m_fps);
    m_status_info->copy_label(info);
    m_statusbar->redraw();
    m_status_at = now_seconds();
}

} // namespace zui::app
