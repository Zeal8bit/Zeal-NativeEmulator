/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Bar.H>

namespace zui::widgets {
class DockLayout;
}

namespace zui::app {

class AppMenus;
class DebuggerUi;
class ScreenView;

/** Main window: menu bar, the screen or the debugger layout, status bar. */
class MainWindow : public Fl_Double_Window {
public:
    MainWindow(bool debug);
    ~MainWindow() override;
    ScreenView* screen() const { return m_screen; }
    /** The debugger UI, nullptr when the debugger is compiled out. */
    DebuggerUi* debugger() const { return m_debugger; }
    bool debug_layout() const { return m_debug; }
    void set_debug_layout(bool debug);
    void apply_window_config(bool debug);
    void capture_window_config(bool debug);
    void scale_screen(float step);
    void set_screen_scale(float scale);
    void toggle_fullscreen();
    /** Force 4:3 was turned on: give the picture area that shape now. */
    void apply_aspect();
    void update_menus();
    /** Rebuild the SNES menu if gamepads or ports changed. */
    void poll_menus();
    void update_status();
    /** Seconds (now_seconds) of the last status update. */
    double status_at() const { return m_status_at; }
    void notify(const char* text);
    void frame_presented();
    void reset_layout();
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;
    /** Normal layout, windowed, with Force 4:3 on. */
    bool aspect_locked() const;
    /** Minimum window size for the current layout. */
    void apply_size_range();
    void layout_status();
    Fl_Menu_Bar* menubar() const { return m_menubar; }
    bool show_fps = false;

    /** Close the UI: the run loop stops at the next frame. */
    static void quit();
    static bool quit_requested();
    static void reset_quit();

private:
    void layout_chrome();
    int chrome_height() const;

    Fl_Menu_Bar* m_menubar = nullptr;
    int m_menubar_h = 0;
    Fl_Group* m_toolbar = nullptr;
    widgets::DockLayout* m_dock = nullptr;
    ScreenView* m_screen = nullptr;
    DebuggerUi* m_debugger = nullptr;
    AppMenus* m_menus = nullptr;
    Fl_Group* m_statusbar = nullptr;
    Fl_Box* m_status_state = nullptr;
    Fl_Box* m_status_msg = nullptr;
    Fl_Box* m_status_info = nullptr;
    bool m_debug = false;
    float m_debug_zoom = 0.0f; /* screen scale in the debugger, 0 = fit */

    double m_fps_epoch = 0;
    int m_fps_frames = 0;
    int m_fps = 0;
    double m_status_at = 0;
};

/** The main window, nullptr before ui_open() and after ui_close(). */
MainWindow* main_window();

} // namespace zui::app
