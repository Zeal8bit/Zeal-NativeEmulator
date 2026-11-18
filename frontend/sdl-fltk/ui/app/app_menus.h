/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The menu bar of the main window: File, Machine, Debug, View, Audio and
 * SNES (rebuilt when gamepads come and go). */
#pragma once

#include <FL/Fl_Menu_Bar.H>

class Fl_Menu_Bar;

namespace zui::app {

class MainWindow;

class AppMenus {
public:
    AppMenus(Fl_Menu_Bar* bar, MainWindow* window);

    /** Sync check marks and enabled states with the current settings. */
    void update();
    void rebuild_snes();
    /** A gamepad or SNES port changed since the last rebuild. */
    static void mark_snes_dirty();
    static bool snes_dirty();

private:
    void build();

    Fl_Menu_Bar* m_bar;
    MainWindow* m_window;
};

} // namespace zui::app
