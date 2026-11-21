/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frontend/sdl-fltk/config.h"
#include "app/app_menus.h"
#include <cstdint>
#include <cstdio>
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Widget.H>
#include "app/debugger_ui.h"
#include "app/main_window.h"
#include "common/machine.h"
#include "common/persist.h"
#include "theme/theme.h"

namespace zui::app {

using namespace zui::widgets;
using namespace zui::common;

namespace {

void cb_save_config(Fl_Widget*, void*)
{
    MainWindow* window = main_window();
    if (!window) return;
    window->capture_window_config(window->debug_layout());
    if (window->debugger()) window->debugger()->capture_layout();
    /* config_save() logs which file failed and leaves it unchanged */
    if (config_save() == 0) notif_show("Configuration saved");
    else notif_show("Configuration not saved: see the log");
}

void cb_quit(Fl_Widget*, void*)
{
    MainWindow::quit();
}

void cb_reset(Fl_Widget*, void*)
{
    fz_reset();
    notif_show("Reset");
}

void cb_toggle_debugger(Fl_Widget*, void*)
{
    if (MainWindow* window = main_window()) {
        window->set_debug_layout(!window->debug_layout());
    }
}

DebuggerUi* debugger_ui()
{
    return main_window() ? main_window()->debugger() : nullptr;
}

/* Reveal the debugger panels for commands issued from the screen layout. */
void debug_command(DebugCommand command)
{
    DebuggerUi* debugger = debugger_ui();
    if (debugger == nullptr) return;
    main_window()->set_debug_layout(true);
    debugger->command(command);
    main_window()->update_status();
}

void cb_continue(Fl_Widget*, void*)  { debug_command(DebugCommand::Continue); }
void cb_pause(Fl_Widget*, void*)     { debug_command(DebugCommand::Pause); }
void cb_step(Fl_Widget*, void*)      { debug_command(DebugCommand::Step); }
void cb_step_over(Fl_Widget*, void*) { debug_command(DebugCommand::StepOver); }
void cb_breakpoint(Fl_Widget*, void*) { debug_command(DebugCommand::ToggleBreakpointAtPc); }

void cb_map_open(Fl_Widget*, void*)
{
    if (DebuggerUi* debugger = debugger_ui()) debugger->open_map(false);
}

void cb_map_add(Fl_Widget*, void*)
{
    if (DebuggerUi* debugger = debugger_ui()) debugger->open_map(true);
}

void cb_map_reload(Fl_Widget*, void*)
{
    if (DebuggerUi* debugger = debugger_ui()) debugger->reload_maps();
}

void cb_pause_on_reset(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    fz_pause_on_reset_set(item && item->value());
}

void cb_passthru(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.debugger.keyboard_passthru = item && item->value();
}

void cb_hex_upper(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.debugger.hex_upper = item && item->value();
    if (main_window() && main_window()->debugger()) main_window()->debugger()->refresh(true);
}

void cb_scale_up(Fl_Widget*, void*)   { if (main_window()) main_window()->scale_screen(1.0f); }
void cb_scale_down(Fl_Widget*, void*) { if (main_window()) main_window()->scale_screen(-1.0f); }
void cb_scale_preset(Fl_Widget*, void* data)
{
    if (main_window()) main_window()->set_screen_scale((float) (intptr_t) data);
}

void cb_aspect(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    config.window.aspect_force = item && item->value();
    if (main_window()) main_window()->apply_aspect();
}

void cb_theme(Fl_Widget*, void* data)
{
    config.window.theme = static_cast<config_theme_t>((intptr_t) data);
    theme::apply_theme_preference();
    if (config_save_user() != 0) notif_show("Theme preference could not be saved");
}

void cb_fullscreen(Fl_Widget*, void*) { if (main_window()) main_window()->toggle_fullscreen(); }

void cb_show_fps(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    if (!main_window()) return;
    main_window()->show_fps = item && item->value();
    main_window()->update_status();
}

void cb_scanline(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    fz_scanline_set(item && item->value());
    notif_show("Scanline rendering: %s", fz_scanline() ? "on" : "off");
}

/* data: ToolId */
void cb_tool(Fl_Widget*, void* data)
{
    DebuggerUi* debugger = debugger_ui();
    if (debugger == nullptr) return;
    const ToolId tool = (ToolId) (intptr_t) data;
    debugger->show_tool(tool, !debugger->tool_shown(tool));
    main_window()->update_menus();
}

void cb_reset_layout(Fl_Widget*, void*)
{
    if (main_window()) main_window()->reset_layout();
}

void change_volume(int delta)
{
    fz_volume_set(fz_volume() + delta);
    if (fz_muted()) fz_muted_set(false);
    notif_show("Volume: %d%%", fz_volume());
    if (main_window()) {
        main_window()->update_menus();
        main_window()->update_status();
    }
}

void cb_volume_up(Fl_Widget*, void*)   { change_volume(10); }
void cb_volume_down(Fl_Widget*, void*) { change_volume(-10); }

void cb_mute(Fl_Widget* w, void*)
{
    const Fl_Menu_Item* item = static_cast<Fl_Menu_*>(w)->mvalue();
    fz_muted_set(item && item->value());
    notif_show(fz_muted() ? "Muted" : "Volume: %d%%", fz_volume());
    if (main_window()) main_window()->update_status();
}

/* SNES port selection: data = (device << 8) | (port + 1); device 0..3 =
 * gamepad index, 0xFF = mouse. */
void cb_snes_port(Fl_Widget*, void* data)
{
    const int value = (int) (intptr_t) data;
    const int device = (value >> 8) & 0xFF;
    const int port = (value & 0xFF) - 1;
    if (device == 0xFF) {
        fz_snes_mouse_set_port(port);
    } else {
        fz_snes_gamepad_set_port(device, port);
    }
    AppMenus::mark_snes_dirty();
}

void cb_snes_mouse_scale(Fl_Widget*, void*)
{
    fz_snes_mouse_reset_scale();
}

void set_toggle(Fl_Menu_Bar* mb, const char* path, bool on, bool active = true)
{
    Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(mb->find_item(path));
    if (item == nullptr) return;
    if (on) item->set(); else item->clear();
    if (active) item->activate(); else item->deactivate();
}

void menu_safe_name(char* out, size_t size, const char* name)
{
    size_t j = 0;
    for (size_t i = 0; name && name[i] && j + 1 < size; i++) {
        const char c = name[i];
        out[j++] = (c == '/' || c == '\\' || c == '&' || c == '_') ? ' ' : c;
    }
    out[j] = '\0';
}

bool s_snes_dirty;

} // namespace

AppMenus::AppMenus(Fl_Menu_Bar* bar, MainWindow* window) : m_bar(bar), m_window(window)
{
    build();
}

void AppMenus::mark_snes_dirty()
{
    s_snes_dirty = true;
}

bool AppMenus::snes_dirty()
{
    return s_snes_dirty;
}

void AppMenus::build()
{
    Fl_Menu_Bar* mb = m_bar;
    mb->add("&File/&Save Config", FL_COMMAND + 's', cb_save_config);
    mb->add("&File/&Quit", FL_COMMAND + 'q', cb_quit);

    mb->add("&Machine/&Reset", FL_COMMAND + FL_SHIFT + FL_BackSpace, cb_reset, nullptr, FL_MENU_DIVIDER);
    if (m_window->debugger()) {
        mb->add("&View/Show &Debugger", FL_COMMAND + (FL_F + 1), cb_toggle_debugger, nullptr, FL_MENU_TOGGLE);

        mb->add("&Debug/&Continue", FL_COMMAND + (FL_F + 5), cb_continue);
        mb->add("&Debug/&Pause", FL_COMMAND + (FL_F + 6), cb_pause);
        mb->add("&Debug/Step &Over", FL_COMMAND + (FL_F + 10), cb_step_over);
        mb->add("&Debug/Step &Into", FL_COMMAND + (FL_F + 11), cb_step);
        mb->add("&Debug/Toggle &Breakpoint at PC", FL_COMMAND + (FL_F + 9), cb_breakpoint, nullptr, FL_MENU_DIVIDER);
        mb->add("&Debug/&Open Map File...", FL_COMMAND + 'o', cb_map_open);
        mb->add("&Debug/&Add Map File...", 0, cb_map_add);
        mb->add("&Debug/&Reload Map Files", 0, cb_map_reload, nullptr, FL_MENU_DIVIDER);
        mb->add("&Debug/Pause After Rese&t", 0, cb_pause_on_reset, nullptr, FL_MENU_TOGGLE | FL_MENU_DIVIDER);
        fz_pause_on_reset_set(preference("debugger", "pause_on_reset", false));
        persist_register([]() {
            set_preference("debugger", "pause_on_reset", fz_pause_on_reset());
        });
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
    const char* const theme_paths[] = { "&View/Theme/System", "&View/Theme/Light", "&View/Theme/Dark" };
    for (int i = THEME_SYSTEM; i <= THEME_DARK; i++) {
        const int checked = config.window.theme == i ? FL_MENU_VALUE : 0;
        mb->add(theme_paths[i], 0, cb_theme, (void*) (intptr_t) i, FL_MENU_RADIO | checked);
    }
    mb->add("&View/Show FPS", 0, cb_show_fps, nullptr, FL_MENU_TOGGLE | FL_MENU_DIVIDER);
    if (m_window->debugger()) {
        mb->add("&View/&Video Window", FL_COMMAND + FL_SHIFT + 'v', cb_tool, (void*) ToolId::Video, FL_MENU_TOGGLE);
        mb->add("&View/&Code && Profiler Window", FL_COMMAND + FL_SHIFT + 'c', cb_tool, (void*) ToolId::Code,
                FL_MENU_TOGGLE);
        mb->add("&View/&Hardware Window", FL_COMMAND + FL_SHIFT + 'h', cb_tool, (void*) ToolId::Hardware,
                FL_MENU_TOGGLE);
        mb->add("&View/Reset Debugger Layout", 0, cb_reset_layout);
    }

    mb->add("&Audio/Volume &Up", FL_COMMAND + FL_SHIFT + '0', cb_volume_up);
    mb->add("&Audio/Volume &Down", FL_COMMAND + FL_SHIFT + '9', cb_volume_down);
    mb->add("&Audio/&Mute", FL_COMMAND + FL_SHIFT + 'm', cb_mute, nullptr, FL_MENU_TOGGLE);

    rebuild_snes();
    update();
}

void AppMenus::update()
{
    Fl_Menu_Bar* mb = m_bar;
    set_toggle(mb, "&View/Show &Debugger", m_window->debug_layout());
    set_toggle(mb, "&Debug/&Keyboard Passthrough", config.debugger.keyboard_passthru);
    set_toggle(mb, "&Debug/&Uppercase Hex", config.debugger.hex_upper);
    set_toggle(mb, "&Debug/Pause After Rese&t", fz_pause_on_reset());
    set_toggle(mb, "&View/Force 4:3 Window", config.window.aspect_force);
    set_toggle(mb, "&View/Scanline Rendering", fz_scanline());
    set_toggle(mb, "&Audio/&Mute", fz_muted());
    set_toggle(mb, "&View/Show FPS", m_window->show_fps);
    if (DebuggerUi* debugger = m_window->debugger()) {
        set_toggle(mb, "&View/&Video Window", debugger->tool_shown(ToolId::Video));
        set_toggle(mb, "&View/&Code && Profiler Window", debugger->tool_shown(ToolId::Code));
        set_toggle(mb, "&View/&Hardware Window", debugger->tool_shown(ToolId::Hardware));
    }
#if FL_API_VERSION >= 10400
    static_cast<Fl_Sys_Menu_Bar*>(mb)->update();
#endif
}

void AppMenus::rebuild_snes()
{
    Fl_Menu_Bar* mb = m_bar;
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

} // namespace zui::app
