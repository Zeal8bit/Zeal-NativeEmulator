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
#include "frontend/sdl-fltk/bridge.h"
#include "debugger/debugger.h"
#include "utils/config.h"
#include "frontend/common/panels.h"
#include "utils/notif.h"
}

namespace zui {

/* ------------------------------------------------------------------ */
/*  Theme (theme.cpp): flat dark surfaces with retro phosphor accents  */
/* ------------------------------------------------------------------ */

constexpr Fl_Color COLOR_BG       = 0x111215'00;  /* window / gaps */
constexpr Fl_Color COLOR_SURFACE  = 0x181A1F'00;  /* panel body */
constexpr Fl_Color COLOR_RAISED   = 0x20232A'00;  /* headers, bars */
constexpr Fl_Color COLOR_BORDER   = 0x2E323B'00;
constexpr Fl_Color COLOR_HOVER    = 0x262A33'00;
constexpr Fl_Color COLOR_TEXT     = 0xD7DAE0'00;
constexpr Fl_Color COLOR_DIM      = 0x5E6575'00;
constexpr Fl_Color COLOR_AMBER    = 0xFFB454'00;  /* headers, labels, PC */
constexpr Fl_Color COLOR_CYAN     = 0x5CCFE6'00;  /* addresses, links */
constexpr Fl_Color COLOR_GREEN    = 0x95E06C'00;  /* running, lit flags */
constexpr Fl_Color COLOR_PC       = 0x2A3320'00;  /* current instruction row */
constexpr Fl_Color COLOR_SELECT   = 0x1F3A57'00;
constexpr Fl_Color COLOR_BREAK    = 0xFF5F57'00;
constexpr Fl_Color COLOR_CHANGED  = 0xFF8FD0'00;
/* Older names kept for the main window */
constexpr Fl_Color COLOR_PANEL    = COLOR_RAISED;
constexpr Fl_Color COLOR_ACCENT   = COLOR_CYAN;
constexpr Fl_Color COLOR_LABEL    = COLOR_AMBER;

constexpr int HEADER_H = 24;

/** Font for data views: BigBlue Terminal (8x16 pixel font) when available. */
extern Fl_Font DATA_FONT;
extern int DATA_SIZE;
extern bool g_pixel_font;

void load_fonts();
/** System monospace font for all UI text (replaces Helvetica). */
void apply_ui_font();
void apply_theme();
int  data_line_height();
/** Panel header: raised strip, amber accent bar, title and optional right text. */
void draw_header(int x, int y, int w, const char* title, const char* extra = nullptr);

enum class Icon { Play, Pause, StepInto, StepOver, RunTo, Reset, Breakpoint };
void draw_icon(Icon icon, int x, int y, int size, Fl_Color color);

class IconButton : public Fl_Button {
public:
    IconButton(int x, int y, int w, int h, Icon icon, const char* text = nullptr);
    void icon(Icon icon) { m_icon = icon; redraw(); }
    void text(const char* text) { m_text = text; redraw(); }
    void icon_color(Fl_Color c) { m_color = c; redraw(); }
    void draw() override;
    int handle(int event) override;

private:
    Icon m_icon;
    const char* m_text;
    Fl_Color m_color = 0;
};

/** Two children separated by a draggable divider. One side keeps a fixed
 *  size (in pixels) while the other takes the remaining space. */
class SplitPane : public Fl_Group {
public:
    static constexpr int GAP = 5;

    SplitPane(int x, int y, int w, int h, bool horizontal, bool fixed_second);
    void set(Fl_Widget* a, Fl_Widget* b, int fixed_size, int min_a = 80, int min_b = 80);
    /** Collapsed: only the first child is shown, filling the pane. */
    void collapse(bool collapsed);
    bool collapsed() const { return m_collapsed; }
    void fixed_size(int size);
    int fixed_size() const { return m_fixed; }
    void on_change(std::function<void()> cb) { m_changed = std::move(cb); }

    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;
    void draw() override;
    void layout();

private:
    int extent() const;
    bool over_divider(int ex, int ey) const;

    Fl_Widget* m_a = nullptr;
    Fl_Widget* m_b = nullptr;
    bool m_horizontal;
    bool m_fixed_second;
    bool m_collapsed = false;
    bool m_dragging = false;
    bool m_cursor_set = false;
    int m_fixed = 200;
    int m_min_a = 80;
    int m_min_b = 80;
    std::function<void()> m_changed;
};

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

/** Main window body: either the screen alone, or the debugger's layout
 *  (which contains the same screen widget). */
class DockLayout : public Fl_Group {
public:
    DockLayout(int x, int y, int w, int h);
    /** Root widget of the layout; `expand(bool)` shows/hides the debugger. */
    void set_root(Fl_Widget* root, std::function<void(bool)> expand);
    void docks_visible(bool visible);
    bool docks_visible() const { return m_docks; }
    void resize(int x, int y, int w, int h) override;

private:
    Fl_Widget* m_root = nullptr;
    std::function<void(bool)> m_expand;
    bool m_docks = false;
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
