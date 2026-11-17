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
#include <FL/Fl_Menu_Bar.H>
#include <cstdint>

extern "C" {
#include "frontend/sdl-fltk/bridge.h"
#include "debugger/debugger.h"
#include "utils/config.h"
#include "frontend/common/panels.h"
#include "utils/notif.h"
}

namespace zui {

/* Palette shared by the custom-drawn widgets */
constexpr Fl_Color COLOR_BG        = 0x1E1F2200;
constexpr Fl_Color COLOR_PANEL     = 0x2B2D3100;
constexpr Fl_Color COLOR_TEXT      = 0xDCDFE400;
constexpr Fl_Color COLOR_DIM       = 0x7F848E00;
constexpr Fl_Color COLOR_ACCENT    = 0x3D8BFD00;
constexpr Fl_Color COLOR_PC        = 0x1F4E8C00;
constexpr Fl_Color COLOR_BREAK     = 0xE0484800;
constexpr Fl_Color COLOR_HOVER     = 0x3A3F4B00;
constexpr Fl_Color COLOR_LABEL     = 0xE5C07B00;
constexpr Fl_Color COLOR_CHANGED   = 0xFF79C600;

constexpr Fl_Font  MONO_FONT       = FL_COURIER;
constexpr int      MONO_SIZE       = 13;

/** Convert an FLTK key code to a zeal_key_t, or 0 (ZEAL_KEY_NULL). */
int fltk_to_zeal_key(int fltk_key);

/** Format helpers honouring the "Hex Upper" preference. */
const char* hex8_fmt();
const char* hex16_fmt();
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
};

/* ------------------------------------------------------------------ */

/** Lays out the screen with optional right and bottom docks, separated by
 *  draggable splitters. Works with any FLTK >= 1.3 (no Fl_Flex/Fl_Tile). */
class DockLayout : public Fl_Group {
public:
    static constexpr int SPLITTER = 5;

    DockLayout(int x, int y, int w, int h);
    void set_children(Fl_Widget* screen, Fl_Widget* right, Fl_Widget* bottom);
    void docks_visible(bool visible);
    bool docks_visible() const { return m_docks; }
    void dock_sizes(int right_w, int bottom_h);
    int right_width() const { return m_right_w; }
    int bottom_height() const { return m_bottom_h; }
    /** Size of the screen area for a given layout size. */
    void screen_size_for(int w, int h, int& sw, int& sh) const;

    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;
    void draw() override;
    void layout();

private:
    enum Drag { DRAG_NONE, DRAG_RIGHT, DRAG_BOTTOM };
    Drag hit(int ex, int ey) const;

    Fl_Widget* m_screen = nullptr;
    Fl_Widget* m_right = nullptr;
    Fl_Widget* m_bottom = nullptr;
    bool m_docks = false;
    int m_right_w = 400;
    int m_bottom_h = 300;
    Drag m_drag = DRAG_NONE;
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

    double m_fps_epoch = 0;
    int m_fps_frames = 0;
    int m_fps = 0;
};

MainWindow* main_window();
double now_seconds();

/* ------------------------------------------------------------------ */
/*  Debugger (debugger.cpp); stubs when the debugger is compiled out   */
/* ------------------------------------------------------------------ */

class Debugger {
public:
    Debugger(DockLayout* dock);
    ~Debugger();

    Fl_Group* toolbar(int x, int y, int w, int h);
    void refresh(bool force);
    void goto_memory(hwaddr addr);
    void goto_disassembly(hwaddr addr);
    void apply_panel_visibility();
    void reset_layout();
    void capture_layout();

    struct Impl;
private:
    Impl* m;
};

} // namespace zui
