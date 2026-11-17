/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Main window, menus, status bar, window persistence, notifications and the
 * ui_* half of bridge.h. */

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>
#include <FL/Fl_Image_Surface.H>
#include "ui.h"

namespace zui {

static MainWindow* s_window;
static bool s_closed;
static bool s_snes_dirty;
static double s_status_at;


constexpr int TOOLBAR_H = 34;
constexpr int STATUS_H = 22;

/* The UI is built before the machine exists: decide on the build flag */
static bool debugger_compiled()
{
#if CONFIG_ENABLE_DEBUGGER
    return true;
#else
    return false;
#endif
}

MainWindow* main_window()
{
    return s_window;
}

double now_seconds()
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

const char* hex8_fmt()
{
    return config.debugger.hex_upper ? "%02X" : "%02x";
}

const char* hex16_fmt()
{
    return config.debugger.hex_upper ? "%04X" : "%04x";
}

bool parse_address(const char* text, hwaddr* out)
{
    if (text == nullptr || out == nullptr) return false;
    while (*text == ' ' || *text == '\t') text++;
    if (*text == '\0') return false;

    const char* digits = text;
    if (*digits == '$') digits++;
    else if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    char* end = nullptr;
    const unsigned long value = std::strtoul(digits, &end, 16);
    if (end != digits) {
        while (*end == ' ' || *end == 'h' || *end == 'H') end++;
        if (*end == '\0') {
            *out = (hwaddr) (value & 0xFFFF);
            return true;
        }
    }
#if CONFIG_ENABLE_DEBUGGER
    dbg_t* dbg = fz_debugger();
    return dbg != nullptr && debugger_find_symbol(dbg, text, out);
#else
    return false;
#endif
}

static float scale_quantize_tenths(float scale, float step)
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

/* ------------------------------------------------------------------ */
/*  Notifications (utils/notif.h)                                      */
/* ------------------------------------------------------------------ */

constexpr double NOTIF_LIFETIME = 1.5;
static char s_notif_text[96];
static bool s_notif_active;

static void notif_expire(void*)
{
    s_notif_active = false;
    if (s_window) {
        s_window->notify("");
        s_window->screen()->redraw();
    }
}

const char* notif_current_text()
{
    return s_notif_active ? s_notif_text : nullptr;
}

/* ------------------------------------------------------------------ */
/*  DockLayout                                                         */
/* ------------------------------------------------------------------ */

DockLayout::DockLayout(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_FLAT_BOX);
    color(COLOR_BG);
    end();
}

void DockLayout::set_root(Fl_Widget* root, std::function<void(bool)> expand)
{
    m_root = root;
    m_expand = std::move(expand);
    if (root->parent() != this) add(root);
    root->resize(x(), y(), w(), h());
    docks_visible(m_docks);
}

void DockLayout::docks_visible(bool visible)
{
    m_docks = visible && m_expand;
    if (m_expand) m_expand(m_docks);
    if (m_root) m_root->resize(x(), y(), w(), h());
    redraw();
}

void DockLayout::resize(int nx, int ny, int nw, int nh)
{
    Fl_Widget::resize(nx, ny, nw, nh);
    if (m_root) m_root->resize(nx, ny, nw, nh);
}

/* ------------------------------------------------------------------ */
/*  Menu callbacks                                                     */
/* ------------------------------------------------------------------ */

static void cb_save_config(Fl_Widget*, void*)
{
    if (!s_window) return;
    s_window->capture_window_config(s_window->debug_layout());
    if (s_window->debugger()) s_window->debugger()->capture_layout();
    if (config_save() == 0) notif_show("Configuration saved");
}

static void cb_quit(Fl_Widget*, void*)
{
    s_closed = true;
    fz_quit();
}

static void cb_reset(Fl_Widget*, void*)
{
    fz_reset();
    notif_show("Reset");
}

static void cb_toggle_debugger(Fl_Widget*, void*)
{
    fz_debugger_set_enabled(!fz_debugger_enabled());
    if (s_window) s_window->update_menus();
}

static void with_debugger(void (*op)(dbg_t*))
{
    dbg_t* dbg = fz_debugger();
    if (dbg == nullptr) return;
    if (!fz_debugger_enabled()) fz_debugger_set_enabled(true);
    op(dbg);
    if (s_window) s_window->update_status();
}

static void cb_continue(Fl_Widget*, void*)  { with_debugger(debugger_continue); }
static void cb_pause(Fl_Widget*, void*)     { with_debugger(debugger_pause); }
static void cb_step(Fl_Widget*, void*)      { with_debugger(debugger_step); }
static void cb_step_over(Fl_Widget*, void*) { with_debugger(debugger_step_over); }
static void cb_breakpoint(Fl_Widget*, void*)
{
    with_debugger(debugger_breakpoint);
    if (s_window && s_window->debugger()) s_window->debugger()->refresh(true);
}

static void cb_passthru(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.debugger.keyboard_passthru = item && item->value();
}

static void cb_hex_upper(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.debugger.hex_upper = item && item->value();
    if (s_window && s_window->debugger()) s_window->debugger()->refresh(true);
}

static void cb_scale_up(Fl_Widget*, void*)   { if (s_window) s_window->scale_screen(1.0f); }
static void cb_scale_down(Fl_Widget*, void*) { if (s_window) s_window->scale_screen(-1.0f); }
static void cb_scale_preset(Fl_Widget*, void* data)
{
    if (s_window) s_window->set_screen_scale((float) (intptr_t) data);
}

static void cb_aspect(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.window.aspect_force = item && item->value();
}

static void cb_fullscreen(Fl_Widget*, void*) { if (s_window) s_window->toggle_fullscreen(); }

static void cb_show_fps(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    if (!s_window) return;
    s_window->show_fps = item && item->value();
    s_window->update_status();
}

static void cb_scanline(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    fz_scanline_set(item && item->value());
    notif_show("Scanline rendering: %s", fz_scanline() ? "on" : "off");
}

#if CONFIG_ENABLE_DEBUGGER
static void cb_video_window(Fl_Widget*, void*)
{
    if (!s_window || !s_window->debugger()) return;
    Debugger* d = s_window->debugger();
    d->show_video_window(!d->video_window_shown());
    s_window->update_menus();
}
#endif

#if CONFIG_ENABLE_DEBUGGER
static void cb_reset_layout(Fl_Widget*, void*)
{
    if (s_window) s_window->reset_layout();
}
#endif

static void change_volume(int delta)
{
    fz_volume_set(fz_volume() + delta);
    if (fz_muted()) fz_muted_set(false);
    notif_show("Volume: %d%%", fz_volume());
    if (s_window) {
        s_window->update_menus();
        s_window->update_status();
    }
}

static void cb_volume_up(Fl_Widget*, void*)   { change_volume(10); }
static void cb_volume_down(Fl_Widget*, void*) { change_volume(-10); }

static void cb_mute(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    fz_muted_set(item && item->value());
    notif_show(fz_muted() ? "Muted" : "Volume: %d%%", fz_volume());
    if (s_window) s_window->update_status();
}

/* SNES port selection: data = (device << 8) | (port + 1); device 0..3 =
 * gamepad index, 0xFF = mouse. */
static void cb_snes_port(Fl_Widget*, void* data)
{
    const int value = (int) (intptr_t) data;
    const int device = (value >> 8) & 0xFF;
    const int port = (value & 0xFF) - 1;
    if (device == 0xFF) {
        fz_snes_mouse_set_port(port);
    } else {
        fz_snes_gamepad_set_port(device, port);
    }
    s_snes_dirty = true;
}

static void cb_snes_mouse_scale(Fl_Widget*, void*)
{
    fz_snes_mouse_reset_scale();
}

static void window_cb(Fl_Widget*, void*)
{
    /* Escape belongs to the emulated machine, never closes the window */
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;
    cb_quit(nullptr, nullptr);
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
    if (debugger_compiled()) {
        m_debugger = new Debugger(m_dock, m_screen);
        begin();
        m_toolbar = m_debugger->toolbar(0, m_menubar_h, w(), TOOLBAR_H);
        end();
        m_toolbar->hide();
    } else {
        m_dock->set_root(m_screen, nullptr);
    }
    end();

    resizable(m_dock);
    size_range(320, 240 + chrome_height());
    build_menus();

    m_debug = !debug; /* force the initial layout */
    set_debug_layout(debug);
}

MainWindow::~MainWindow()
{
    delete m_debugger;
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
    init_sizes();
    redraw();
}

void MainWindow::resize(int nx, int ny, int nw, int nh)
{
    Fl_Double_Window::resize(nx, ny, nw, nh);
}

int MainWindow::handle(int event)
{
    if (event == FL_UNFOCUS || event == FL_HIDE) fz_keys_release_all();
    return Fl_Double_Window::handle(event);
}

void MainWindow::set_debug_layout(bool debug)
{
    if (debug && m_debugger == nullptr) debug = false;
    if (debug == m_debug) return;
    m_debug = debug;
    label(debug ? FZ_WIN_NAME " - Debugger" : FZ_WIN_NAME);
    m_dock->docks_visible(debug);
    layout_chrome();
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


void MainWindow::build_menus()
{
    Fl_Menu_Bar* mb = m_menubar;
    mb->add("&File/&Save Config", FL_COMMAND + 's', cb_save_config);
    mb->add("&File/&Quit", FL_COMMAND + 'q', cb_quit);

    mb->add("&Machine/&Reset", FL_COMMAND + FL_SHIFT + FL_BackSpace, cb_reset, nullptr, FL_MENU_DIVIDER);
    if (debugger_compiled()) {
        mb->add("&Machine/&Debugger", FL_COMMAND + (FL_F + 1), cb_toggle_debugger, nullptr, FL_MENU_TOGGLE);

        mb->add("&Debug/&Continue", FL_COMMAND + (FL_F + 5), cb_continue);
        mb->add("&Debug/&Pause", FL_COMMAND + (FL_F + 6), cb_pause);
        mb->add("&Debug/Step &Over", FL_COMMAND + (FL_F + 10), cb_step_over);
        mb->add("&Debug/Step &Into", FL_COMMAND + (FL_F + 11), cb_step);
        mb->add("&Debug/Toggle &Breakpoint at PC", FL_COMMAND + (FL_F + 9), cb_breakpoint, nullptr, FL_MENU_DIVIDER);
        mb->add("&Debug/&Keyboard Passthrough", 0, cb_passthru, nullptr, FL_MENU_TOGGLE);
        mb->add("&Debug/&Uppercase Hex", 0, cb_hex_upper, nullptr, FL_MENU_TOGGLE);
    }

    mb->add("&View/Scale &Up", FL_COMMAND + FL_SHIFT + '=', cb_scale_up);
    mb->add("&View/Scale &Down", FL_COMMAND + FL_SHIFT + '-', cb_scale_down);
    mb->add("&View/Scale 1x", 0, cb_scale_preset, (void*) (intptr_t) 1);
    mb->add("&View/Scale 2x", 0, cb_scale_preset, (void*) (intptr_t) 2);
    mb->add("&View/Scale 3x", 0, cb_scale_preset, (void*) (intptr_t) 3, FL_MENU_DIVIDER);
    mb->add("&View/Force 4:3 Window", 0, cb_aspect, nullptr, FL_MENU_TOGGLE);
    mb->add("&View/&Fullscreen", FL_COMMAND + FL_SHIFT + 'f', cb_fullscreen, nullptr, FL_MENU_DIVIDER);
    if (fz_scanline_available()) {
        mb->add("&View/Scanline Rendering", 0, cb_scanline, nullptr, FL_MENU_TOGGLE);
    }
    mb->add("&View/Show FPS", 0, cb_show_fps, nullptr, FL_MENU_TOGGLE | FL_MENU_DIVIDER);
#if CONFIG_ENABLE_DEBUGGER
    mb->add("&View/&Video && Profiler Window", FL_COMMAND + FL_SHIFT + 'v', cb_video_window, nullptr, FL_MENU_TOGGLE);
    mb->add("&View/Reset Debugger Layout", 0, cb_reset_layout);
#endif

    mb->add("&Audio/Volume &Up", FL_COMMAND + FL_SHIFT + '0', cb_volume_up);
    mb->add("&Audio/Volume &Down", FL_COMMAND + FL_SHIFT + '9', cb_volume_down);
    mb->add("&Audio/&Mute", FL_COMMAND + FL_SHIFT + 'm', cb_mute, nullptr, FL_MENU_TOGGLE);

    rebuild_snes_menu();
    update_menus();
}

static void set_toggle(Fl_Menu_Bar* mb, const char* path, bool on, bool active = true)
{
    Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(mb->find_item(path));
    if (item == nullptr) return;
    if (on) item->set(); else item->clear();
    if (active) item->activate(); else item->deactivate();
}

void MainWindow::update_menus()
{
    Fl_Menu_Bar* mb = m_menubar;
    const bool dbg = fz_debugger_enabled();
    set_toggle(mb, "&Machine/&Debugger", dbg);
    set_toggle(mb, "&Debug/&Keyboard Passthrough", config.debugger.keyboard_passthru);
    set_toggle(mb, "&Debug/&Uppercase Hex", config.debugger.hex_upper);
    set_toggle(mb, "&View/Force 4:3 Window", config.window.aspect_force);
    set_toggle(mb, "&View/Scanline Rendering", fz_scanline());
    set_toggle(mb, "&View/Show FPS", show_fps);
    set_toggle(mb, "&Audio/&Mute", fz_muted());
    set_toggle(mb, "&View/&Video && Profiler Window",
               m_debugger && m_debugger->video_window_shown(), m_debugger != nullptr);
#if FL_API_VERSION >= 10400
    static_cast<Fl_Sys_Menu_Bar*>(mb)->update();
#endif
}

static void menu_safe_name(char* out, size_t size, const char* name)
{
    size_t j = 0;
    for (size_t i = 0; name && name[i] && j + 1 < size; i++) {
        const char c = name[i];
        out[j++] = (c == '/' || c == '\\' || c == '&' || c == '_') ? ' ' : c;
    }
    out[j] = '\0';
}

void MainWindow::rebuild_snes_menu()
{
    Fl_Menu_Bar* mb = m_menubar;
    const int root = mb->find_index("&SNES");
    if (root >= 0) mb->clear_submenu(root);

    char path[160];
    char name[64];
    const char* port_names[] = { "Detached", "Port 1", "Port 2" };
    bool any = false;
    for (int i = 0; i < fz_snes_gamepad_count(); i++) {
        if (!fz_snes_gamepad_available(i)) continue;
        any = true;
        menu_safe_name(name, sizeof(name), fz_snes_gamepad_name(i));
        const int current = fz_snes_gamepad_port(i);
        for (int p = -1; p < 2; p++) {
            snprintf(path, sizeof(path), "&SNES/%d: %s/%s", i + 1, name, port_names[p + 1]);
            const int idx = mb->add(path, 0, cb_snes_port, (void*) (intptr_t) ((i << 8) | (p + 1)), FL_MENU_RADIO);
            Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(&mb->menu()[idx]);
            if (p == current) item->set(); else item->clear();
        }
    }
    if (!any) {
        mb->add("&SNES/No gamepad connected", 0, nullptr, nullptr, FL_MENU_INACTIVE);
    }
    const int current = fz_snes_mouse_port();
    for (int p = -1; p < 2; p++) {
        snprintf(path, sizeof(path), "&SNES/Mouse/%s", port_names[p + 1]);
        const int idx = mb->add(path, 0, cb_snes_port, (void*) (intptr_t) ((0xFF << 8) | (p + 1)), FL_MENU_RADIO);
        Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(&mb->menu()[idx]);
        if (p == current) item->set(); else item->clear();
    }
    mb->add("&SNES/Mouse/Reset Speed", 0, cb_snes_mouse_scale, nullptr, 0);
    s_snes_dirty = false;
#if FL_API_VERSION >= 10400
    static_cast<Fl_Sys_Menu_Bar*>(mb)->update();
#endif
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
    char state[64];
    snprintf(state, sizeof(state), fz_debugger_enabled() ? "Running (debugger)" : "Running");
#if CONFIG_ENABLE_DEBUGGER
    dbg_t* dbg = fz_debugger();
    if (fz_paused() && dbg) {
        regs_t regs;
        debugger_get_registers(dbg, &regs);
        char pc[8];
        snprintf(pc, sizeof(pc), hex16_fmt(), regs.pc);
        snprintf(state, sizeof(state), "Paused at $%s", pc);
    }
#endif
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
    s_status_at = now_seconds();
}

} // namespace zui

/* ------------------------------------------------------------------ */
/*  C interfaces                                                       */
/* ------------------------------------------------------------------ */

using namespace zui;

extern "C" {

void notif_reset(void)
{
    s_notif_active = false;
    s_notif_text[0] = '\0';
}

void notif_show(const char* fmt, ...)
{
    if (fmt == nullptr) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_notif_text, sizeof(s_notif_text), fmt, args);
    va_end(args);
    s_notif_active = true;
    if (s_window) {
        s_window->notify(s_notif_text);
        s_window->screen()->redraw();
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

void config_window_update(bool dbg_enabled)
{
    if (s_window) s_window->capture_window_config(dbg_enabled);
    if (s_window && s_window->debugger()) s_window->debugger()->capture_layout();
}

void config_window_set(bool dbg_enabled)
{
    if (s_window) s_window->apply_window_config(dbg_enabled);
}

bool config_keyboard_passthru(bool dbg_enabled)
{
    return dbg_enabled && config.debugger.keyboard_passthru;
}

int ui_open(bool debug)
{
#if CONFIG_ENABLE_DEBUGGER
    for (int i = 0; i < FRONTEND_PANEL_COUNT; i++) {
        config_panel_load(&config_panels[i]);
    }
#endif

    /* Cocoa (macOS system menubar) must exist before any menu is built */
    fl_open_display();
#ifdef __APPLE__
    Fl::set_font(FL_COURIER, "Menlo");
#endif
    apply_theme();
    load_fonts();
#if FL_API_VERSION >= 10400
    Fl::use_high_res_GL(1);
#endif
    Fl::visible_focus(0);

    s_closed = false;
    MainWindow* window = new MainWindow(debug);
    window->show();
    window->screen()->show();
    window->screen()->take_focus();
    Fl::check();
    return 0;
}

void ui_close(void)
{
    Fl::remove_timeout(notif_expire);
    if (s_window == nullptr) return;
    s_window->screen()->lock_pointer(false);
    s_window->hide();
    delete s_window;
    s_window = nullptr;
    Fl::check();
}

bool ui_closed(void)
{
    return s_closed || s_window == nullptr;
}

/* Developer aid: ZEAL_FLTK_SCREENSHOT=<file.ppm> saves the main window after
 * ZEAL_FLTK_SCREENSHOT_DELAY seconds (default 3), to check the UI headlessly. */
static void maybe_screenshot()
{
#if FL_API_VERSION >= 10400
    static bool done;
    const char* path = getenv("ZEAL_FLTK_SCREENSHOT");
    if (done || path == nullptr || s_window == nullptr) return;
    const char* delay = getenv("ZEAL_FLTK_SCREENSHOT_DELAY");
    if (now_seconds() < (delay ? atof(delay) : 3.0)) return;
    done = true;
    Fl_Image_Surface surface(s_window->w(), s_window->h(), 1);
    Fl_Surface_Device::push_current(&surface);
    surface.draw(s_window);
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

void ui_wait(double seconds)
{
    if (s_window == nullptr) return;
    maybe_screenshot();
    if (s_snes_dirty) s_window->rebuild_snes_menu();
    if (now_seconds() - s_status_at > 0.25) s_window->update_status();
    Fl::wait(seconds);
}

void ui_present(const uint16_t* pixels, int width, int height)
{
    if (s_window == nullptr) return;
    s_window->screen()->present(pixels, width, height);
    s_window->frame_presented();
}

void ui_debug_mode(bool enabled)
{
    if (s_window) s_window->set_debug_layout(enabled);
}

void ui_debug_refresh(bool force)
{
    if (s_window && s_window->debugger() && s_window->debug_layout()) {
        s_window->debugger()->refresh(force);
        if (force) s_window->update_status();
    }
}

bool ui_screen_focused(void)
{
    return s_window && Fl::focus() == s_window->screen();
}

void ui_screen_scale(float* sx, float* sy)
{
    float x = 1.0f;
    float y = 1.0f;
    if (s_window) {
        int px, py, pw, ph;
        s_window->screen()->picture_rect(px, py, pw, ph);
        if (pw > 0) x = (float) FZ_SCREEN_WIDTH / pw;
        if (ph > 0) y = (float) FZ_SCREEN_HEIGHT / ph;
    }
    if (sx) *sx = x;
    if (sy) *sy = y;
}

void ui_pointer_lock(bool locked)
{
    if (s_window) s_window->screen()->lock_pointer(locked);
}

void ui_gamepads_changed(void)
{
    s_snes_dirty = true;
}

} // extern "C"
