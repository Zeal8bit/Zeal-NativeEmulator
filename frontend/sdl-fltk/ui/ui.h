/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private C++ declarations of the FLTK user interface. */
#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <cstdint>
#include <functional>

extern "C" {
#include "frontend/sdl-fltk/bridge/machine.h"
#include "debugger/debugger.h"
#include "utils/config.h"
#include "frontend/common/panels.h"
#include "utils/notif.h"
}

#include "theme/theme.h"
#include "theme/fonts.h"
#include "theme/style.h"
#include "common/clock.h"
#include "common/format.h"
#include "common/machine.h"
#include "common/persist.h"
#include "widgets/dock_layout.h"
#include "widgets/icon_button.h"
#include "widgets/panel.h"
#include "widgets/split_pane.h"
namespace zui {
using namespace zui::theme;
using namespace zui::common;
using namespace zui::widgets;
}

namespace zui {

/** Convert an FLTK key code to a zeal_key_t, or 0 (ZEAL_KEY_NULL). */
int fltk_to_zeal_key(int fltk_key);

/** Parse a hex address ("1234", "$1234", "0x1234") or a debugger symbol. */
bool parse_address(const char* text, hwaddr* out);

/* ------------------------------------------------------------------ */

class ScreenView : public Fl_Gl_Window {
public:
    ScreenView(int x, int y, int w, int h);
    ~ScreenView() override;

    void present(const uint16_t* pixels, int width, int height);
    /** Rectangle (widget coordinates) in which the 4:3 picture is drawn. */
    void picture_rect(int& x, int& y, int& w, int& h) const;
    /** Picture scale inside the widget (1 = 640x480); 0 fits the widget.
     *  Never drawn larger than what fits. */
    void zoom(float scale);
    float zoom() const { return m_zoom; }
    /** Largest scale that fits the widget. */
    float fit_scale() const;
    void lock_pointer(bool locked);
    bool pointer_locked() const { return m_locked; }

    int handle(int event) override;
    void draw() override;

private:
    void pointer_motion();
    void reset_texture();

    const uint16_t* m_pixels = nullptr;
    int m_frame_w = FZ_SCREEN_WIDTH;
    int m_frame_h = FZ_SCREEN_HEIGHT;
    unsigned m_texture = 0;
    bool m_locked = false;
    int m_last_x = -1;
    int m_last_y = -1;
    float m_zoom = 0.0f;
};

/* ------------------------------------------------------------------ */

class Debugger;

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(bool debug);
    ~MainWindow() override;

    ScreenView* screen() const { return m_screen; }
    Debugger* debugger() const { return m_debugger; }
    bool debug_layout() const { return m_debug; }

    void set_debug_layout(bool debug);
    void apply_window_config(bool debug);
    void capture_window_config(bool debug);
    void scale_screen(float step);
    void set_screen_scale(float scale);
    void toggle_fullscreen();

    void update_menus();
    void rebuild_snes_menu();
    void update_status();
    void notify(const char* text);
    void frame_presented();
    void reset_layout();

    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    Fl_Menu_Bar* menubar() const { return m_menubar; }
    bool show_fps = false;

private:
    void build_menus();
    void layout_chrome();
    int chrome_height() const;

    Fl_Menu_Bar* m_menubar = nullptr;
    int m_menubar_h = 0;
    Fl_Group* m_toolbar = nullptr;
    DockLayout* m_dock = nullptr;
    ScreenView* m_screen = nullptr;
    Debugger* m_debugger = nullptr;
    Fl_Group* m_statusbar = nullptr;
    Fl_Box* m_status_state = nullptr;
    Fl_Box* m_status_msg = nullptr;
    Fl_Box* m_status_info = nullptr;
    bool m_debug = false;
    float m_debug_zoom = 0.0f; /* screen scale in the debugger, 0 = fit */

    double m_fps_epoch = 0;
    int m_fps_frames = 0;
    int m_fps = 0;
};

MainWindow* main_window();

/* ------------------------------------------------------------------ */
/*  Debugger (debugger.cpp); stubs when the debugger is compiled out   */
/* ------------------------------------------------------------------ */

class Debugger {
public:
    /** Builds the debugger layout around `screen` inside `dock`. */
    Debugger(DockLayout* dock, ScreenView* screen);
    ~Debugger();

    Fl_Group* toolbar(int x, int y, int w, int h);
    void refresh(bool force);
    void goto_memory(hwaddr addr);
    void goto_disassembly(hwaddr addr);
    /** Window size that fits the default debugger layout (body only). */
    void default_body_size(int& w, int& h) const;
    /** Restore/save split sizes from/to config_panels. */
    void apply_layout();
    void capture_layout();
    void reset_layout();
    /** Separate Video & Profiler tool window. */
    void show_video_window(bool show);
    bool video_window_shown() const;

    struct Impl;
private:
    Impl* m;
};

} // namespace zui
