// SPDX-License-Identifier: Apache-2.0
// The debugger window shell: menu, toolbar, status bar, dock workspace, floating panel
// windows, theming and layout. The C entry points in frontend.cpp drive this class.
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/cp437.h"
#include "ui/fltk/input.h"
#include "ui/fltk/panel.h"
#include "ui/fltk/panels/breakpoints.h"
#include "ui/fltk/panels/cpu.h"
#include "ui/fltk/panels/disassembler.h"
#include "ui/fltk/panels/memory.h"
#include "ui/fltk/panels/mmu.h"
#include "ui/fltk/panels/semihost.h"
#include "ui/fltk/panels/video.h"
#include "ui/fltk/panels/vram.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/dock_surface.h"
#include "ui/fltk/widgets/float_window.h"
#include "ui/fltk/widgets/text_input.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Grid.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Table.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <vector>

using zeal_ui::DockNode;
using zeal_ui::Floating;
using zeal_ui::Theme;
using zeal_ui::Workspace;
using zeal_ui::address;
using zeal_ui::glyph;
using zeal_ui::hex;
using zeal_ui::hex_digit;
using zeal_ui::now;
using zeal_ui::text;


void dbg_ui_t::changed_layout()
{
    // FLTK may redraw before the outer event pump returns. Drop pointers into
    // the old tree immediately; widget reparenting happens after callbacks.
    surface->leaves.clear();
    surface->splits.clear();
    surface->resizing = nullptr;
    rebuild = true;
    menu_dirty = true; // View check marks track panel visibility
}

void dbg_ui_t::drop(int x, int y)
{
    release();
    int mx = x - window->x(), my = y - window->y();
    for (DockSurface::Leaf l : surface->leaves)
        if (l.rect.contains(mx, my)) {
            int target = l.node->tabs[l.node->selected], edge = DockSurface::target_edge(l.rect, mx, my);
            if (my < l.rect.y + theme.row_height) {
                int tab_width = std::max(32, (l.rect.w - 24) / (int)l.node->tabs.size());
                target = l.node->tabs[std::min(int(l.node->tabs.size()) - 1, (mx - l.rect.x) / tab_width)];
                edge = 0;
            }
            if (target == drag && edge)
                for (int id : l.node->tabs)
                    if (id != drag) {
                        target = id;
                        break;
                    }
            workspace.dock(drag, target, edge);
            changed_layout();
            return;
        }
    workspace.detach(drag, x, y);
    changed_layout();
}

void dbg_ui_t::hide_panel(int id)
{
    release();
    workspace.hide(id);
    changed_layout();
}

void dbg_ui_t::layout()
{
    // Carry the on-screen geometry of windows that are still floating back into the model.
    for (Floating &f : workspace.floating)
        if (floating.count(f.panel)) {
            Fl_Double_Window *w = floating.at(f.panel);
            f.x = w->x();
            f.y = w->y();
            f.w = w->w();
            f.h = w->h();
        }
    release();
    bool keep[8] = {};
    for (Floating &f : workspace.floating)
        keep[f.panel] = true;
    // Close only the windows whose panel is no longer floating. Rebuilding every window
    // here would flash the whole desktop whenever one panel is closed or re-docked.
    for (auto it = floating.begin(); it != floating.end();) {
        if (keep[it->first]) {
            ++it;
            continue;
        }
        Panel *p = panels[it->first];
        if (p->parent())
            p->parent()->remove(p); // the panel outlives its window
        it->second->hide();
        delete it->second;
        it = floating.erase(it);
    }
    // Detach panels that are about to be re-docked; floating panels keep their window.
    for (Panel *p : panels)
        if (!keep[p->id]) {
            p->hide();
            if (p->parent())
                p->parent()->remove(p);
        }
    for (Floating &f : workspace.floating) {
        if (floating.count(f.panel))
            continue; // already has a window
        int sx, sy, sw, sh;
        Fl::screen_work_area(sx, sy, sw, sh, f.x, f.y);
        f.w = std::clamp(f.w, 160, sw);
        f.h = std::clamp(f.h, 100, sh);
        f.x = std::clamp(f.x, sx, sx + sw - f.w);
        f.y = std::clamp(f.y, sy, sy + sh - f.h);
        FloatWindow *w = new FloatWindow(this, f.panel, f.x, f.y, f.w, f.h);
        w->screen_num(Fl::screen_num(f.x, f.y, f.w, f.h));
        w->size_range(200, 160);
        w->begin();
        w->add(panels[f.panel]);
        w->end();
        panels[f.panel]->resize(0, theme.row_height, f.w, f.h - theme.row_height);
        w->resizable(panels[f.panel]);
        w->callback(
            [](Fl_Widget *w, void *data) {
                dbg_ui_t *u = (dbg_ui_t *)data;
                for (auto [id, f] : u->floating)
                    if (f == w) {
                        u->hide_panel(id);
                        break;
                    }
            },
            this);
        floating[f.panel] = w;
        panels[f.panel]->show();
        if (shown)
            w->show();
    }
    surface->update();
    apply_theme();
    // The View check marks and every panel toggle are read while the menu is built, so any
    // layout pass invalidates them. Without this the first menu of a session describes the
    // default workspace instead of the one just loaded from fltk-workspace.ini.
    menu_dirty = true;
    rebuild = false;
}

static Fl_Font resolve_font(const std::string &name, Fl_Font fallback)
{
    int count = Fl::set_fonts(nullptr);
    for (int i = 0; i < count; i++)
        if (name == Fl::get_font_name((Fl_Font)i))
            return (Fl_Font)i;
    return fallback;
}

void dbg_ui_t::apply_theme()
{
    ui_font = resolve_font(theme.ui_font, FL_HELVETICA);
    mono_font = resolve_font(theme.mono_font, FL_COURIER);
    unsigned bg = theme.colors.at("background"), fg = theme.colors.at("text"),
         surface_color = theme.colors.at("surface");
    Fl::background(bg >> 16, (bg >> 8) & 255, bg & 255);
    Fl::foreground(fg >> 16, (fg >> 8) & 255, fg & 255);
    Fl::background2(surface_color >> 16, (surface_color >> 8) & 255, surface_color & 255);
    std::function<void(Fl_Widget *)> style = [&](Fl_Widget *w) {
        w->color(color("surface"));
        w->labelcolor(color("text"));
        w->selection_color(color("selection"));
        w->labelfont(ui_font);
        w->labelsize(theme.font_size);
        if (Fl_Input *i = dynamic_cast<Fl_Input *>(w)) {
            i->textcolor(color("text"));
            i->textfont(mono_font);
            i->textsize(theme.mono_size);
            i->cursor_color(color("link"));
        }
        if (Fl_Menu_ *m = dynamic_cast<Fl_Menu_ *>(w)) {
            m->textcolor(color("text"));
            m->textfont(ui_font);
            m->textsize(theme.font_size);
        }
        if (Fl_Table *t = dynamic_cast<Fl_Table *>(w)) {
            t->table_box(FL_DOWN_BOX);
            t->col_header_color(color("background"));
            t->row_header_color(color("background"));
        }
        // The generic pass flattens scrollbars to the surface color, which hides
        // them. Give the trough and handle distinct theme roles instead.
        if (Fl_Scrollbar *s = dynamic_cast<Fl_Scrollbar *>(w)) {
            s->color(color("background"));
            s->selection_color(color("border"));
            s->labelcolor(color("text"));
        }
        if (Fl_Group *g = dynamic_cast<Fl_Group *>(w))
            for (int i = 0; i < g->children(); i++)
                style(g->child(i));
    };
    int row = theme.row_height, pad = theme.spacing;
    shell->gap(pad);
    shell->fixed(menu, row + 4);
    shell->fixed(toolbar_row, row);
    shell->fixed(status_bar, row);
    toolbar_row->gap(pad);
    for (unsigned i = 0; i < toolbar.size(); i++)
        if (toolbar[i])
            toolbar_row->fixed(toolbar[i], row * 4 / 3);
    shell->layout();
    style(window);
    for (Fl_Box *f : status_fields)
        f->color(color("background"));
    for (Fl_Box *s : status_separators)
        s->color(color("border"));
    status_fields[ST_PC]->labelcolor(color("text"));
    status_fields[ST_FRAME]->labelcolor(color("muted"));
    status_fields[ST_FPS]->labelcolor(color("muted"));
    status_fields[ST_MESSAGE]->labelcolor(color("link"));
    status_fields[ST_MACHINE]->labelcolor(color("muted"));
    status_dot->labelcolor(color("success"));
    status_dot->labelfont(mono_font); // the UI font has no U+25CF
    apply_icons();
    layout_status();
    for (auto [id, w] : floating) {
        style(w);
        panels[id]->resize(0, theme.row_height, w->w(), w->h() - theme.row_height);
        w->redraw();
    }
    for (Panel *p : panels)
        p->resize(p->x(), p->y(), p->w(), p->h());
    surface->update();
    window->redraw();
}

void dbg_ui_t::select_theme(const std::string &name)
{
    Theme next;
    std::string error;
    if (name == "Dark" || name == "Light")
        next = Theme::preset(name == "Light");
    else if (!Theme::load(directory + "/themes/" + name, next, error)) {
        message = error;
        return;
    }
    theme = std::move(next);
    theme_name = name;
    apply_theme();
}

void dbg_ui_t::save()
{
    for (Floating &f : workspace.floating) {
        Fl_Double_Window *w = floating.at(f.panel);
        f.x = w->x();
        f.y = w->y();
        f.w = w->w();
        f.h = w->h();
    }
    // While the debugger is off the live tree only holds the video, so writing it out
    // would throw away the panel arrangement the user expects back next launch.
    const zeal_ui::Workspace &persisted = debug_workspace_saved ? debug_workspace : workspace;
    if (!persisted.save(directory + "/fltk-workspace.ini")) {
        message = "Cannot save workspace";
        return;
    }
    std::ofstream f(directory + "/fltk-preferences.ini");
    f << "theme=" << theme_name << "\nwidth=" << window->w() << "\nheight=" << window->h()
      << "\nupper=" << upper << "\ncp437=" << cp437 << "\n";
    message = f ? "Workspace and theme saved" : "Cannot save preferences";
}

void dbg_ui_t::release()
{
    // FLTK menus own the same global grab, so only the video panel's capture is dropped.
    if (panels[zeal_ui::PANEL_VIDEO])
        panels[zeal_ui::PANEL_VIDEO]->release_capture();
    host.release_input(host.debugger);
}

void dbg_ui_t::navigate(uint32_t addr)
{
    const int id = zeal_ui::PANEL_MEMORY;
    panels[id]->go_to_address(addr);
    workspace.show(id);
    if (DockNode *n = workspace.leaf(id))
        n->selected = std::find(n->tabs.begin(), n->tabs.end(), id) - n->tabs.begin();
    changed_layout();
    last_refresh = 0;
}

static void menu_callback(Fl_Widget *w, void *data)
{
    dbg_ui_t *u = (dbg_ui_t *)data;
    Fl_Menu_Bar *menu = (Fl_Menu_Bar *)w;
    char path[512];
    menu->item_pathname(path, sizeof(path));
    std::string s = path;
    if (s == "File/Debugger Off") {
        u->save();
        u->host.action(u->host.debugger, UI_OFF, 0, 0);
    } else if (s == "File/Save Config") {
        u->save();
        u->host.action(u->host.debugger, UI_SAVE, 0, 0);
    } else if (s == "File/Quit")
        u->command(DBG_STOP);
    else if (s == "CPU/Pause")
        u->command(DBG_PAUSE);
    else if (s == "CPU/Continue")
        u->command(DBG_CONTINUE);
    else if (s == "CPU/Step")
        u->command(DBG_STEP);
    else if (s == "CPU/Step Over")
        u->command(DBG_STEP_OVER);
    else if (s == "CPU/Reset")
        u->command(DBG_RESET);
    else if (s == "CPU/Toggle Breakpoint") {
        debugger_toggle_breakpoint(u->host.debugger, u->snapshot.pc);
        u->last_refresh = 0;
    } else if (s == "View/Reset Layout") {
        u->workspace.reset();
        u->separate_mode = false;
        u->changed_layout();
    } else if (s == "View/Separate Panel Windows") {
        u->set_separate_windows(!u->separate_windows());
    } else if (s == "View/Keyboard Passthrough") {
        u->passthrough = !u->passthrough;
        u->release();
        u->host.action(u->host.debugger, UI_PASSTHROUGH, u->passthrough, 0);
    } else if (s == "View/Uppercase Hex") {
        u->upper = !u->upper;
        u->last_refresh = 0;
    } else if (s == "View/CP437") {
        u->cp437 = !u->cp437;
        u->last_refresh = 0;
    } else if (s.rfind("View/", 0) == 0) {
        for (int i = 0; i < 8; i++)
            if (s == std::string("View/") + zeal_ui::panel_name(i)) {
                if (u->panel_open(i))
                    u->hide_panel(i); // the View item toggles visibility
                else if (u->separate_mode)
                    u->detach_panel(i);
                else {
                    u->workspace.show(i);
                    if (DockNode *n = u->workspace.leaf(i))
                        n->selected = std::find(n->tabs.begin(), n->tabs.end(), i) - n->tabs.begin();
                    u->changed_layout();
                }
            }
    } else if (s == "Theme/Reload")
        u->select_theme(u->theme_name);
    else if (s.rfind("Theme/", 0) == 0)
        u->select_theme(s.substr(6));
    else if (s == "Video/Scale Up")
        u->scale = std::min(6, u->scale + 1);
    else if (s == "Video/Scale Down")
        u->scale = std::max(1, u->scale - 1);
    else if (s == "Audio/Volume Up" || s == "Audio/Volume Down") {
        u->volume = std::clamp(u->volume + (s == "Audio/Volume Up" ? 10 : -10), 0, 100);
        u->host.action(u->host.debugger, UI_VOLUME, u->volume, 0);
        u->message = "Volume " + std::to_string(u->volume) + "%";
    } else if (s == "SNES/Reset Mouse Speed")
        u->host.action(u->host.debugger, UI_MOUSE_RESET, 0, 0);
}

void dbg_ui_t::refresh_snes()
{
    if (!host.snes_state || Fl::grab())
        return;
    dbg_snes_state_t current;
    host.snes_state(host.debugger, &current);
    if (std::memcmp(&current, &snes, sizeof(current)))
        build_menu();
}

void dbg_ui_t::build_menu()
{
    menu->clear();
    if (host.snes_state)
        host.snes_state(host.debugger, &snes);
    else {
        snes = {};
        std::fill(std::begin(snes.ports), std::end(snes.ports), DBG_SNES_DETACHED);
    }
    snes_choices.clear();
    snes_choices.reserve(DBG_SNES_PORTS * (DBG_HOST_GAMEPADS + 2));
    auto add = [&](const std::string &s, int key = 0, int flags = 0) {
        menu->add(s.c_str(), key, menu_callback, this, flags);
    };
    add("File/Debugger Off", FL_COMMAND + FL_F + 1);
    add("File/Save Config", FL_COMMAND + 's');
    add("File/Quit", FL_COMMAND + 'q');
    add("CPU/Continue", FL_COMMAND + FL_F + 5);
    add("CPU/Pause", FL_COMMAND + FL_F + 6);
    add("CPU/Step Over", FL_COMMAND + FL_F + 10);
    add("CPU/Step", FL_COMMAND + FL_F + 11);
    add("CPU/Toggle Breakpoint", FL_COMMAND + FL_F + 9);
    add("CPU/Reset", FL_COMMAND + FL_SHIFT + FL_BackSpace);
    for (int i = 0; i < 8; i++)
        add(std::string("View/") + zeal_ui::panel_name(i), 0, FL_MENU_TOGGLE | (panel_open(i) ? FL_MENU_VALUE : 0));
    add("View/Reset Layout");
    add("View/Separate Panel Windows", 0, FL_MENU_TOGGLE | (separate_windows() ? FL_MENU_VALUE : 0));
    add("View/Keyboard Passthrough", 0, FL_MENU_TOGGLE | (passthrough ? FL_MENU_VALUE : 0));
    add("View/Uppercase Hex", 0, FL_MENU_TOGGLE | (upper ? FL_MENU_VALUE : 0));
    add("View/CP437", 0, FL_MENU_TOGGLE | (cp437 ? FL_MENU_VALUE : 0));
    add("Theme/Dark");
    add("Theme/Light");
    add("Theme/Reload");
    std::error_code ec;
    std::filesystem::create_directories(directory + "/themes", ec);
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory + "/themes", ec))
        if (entry.path().extension() == ".ini")
            add("Theme/" + entry.path().filename().string());
    add("Video/Scale Up", FL_COMMAND + FL_SHIFT + '=');
    add("Video/Scale Down", FL_COMMAND + FL_SHIFT + '-');
    add("Audio/Volume Up", FL_COMMAND + FL_SHIFT + '0');
    add("Audio/Volume Down", FL_COMMAND + FL_SHIFT + '9');
    for (int port = 0; port < DBG_SNES_PORTS; ++port) {
        auto option = [&](const std::string &label, int device, bool available = true) {
            std::string escaped;
            for (char c : label) {
                if (c == '/' || c == '\\')
                    escaped += '\\';
                if (c == '&')
                    escaped += '&';
                escaped += c;
            }
            snes_choices.push_back({this, port, device});
            const std::string path = "SNES/Port " + std::to_string(port + 1) + "/" + escaped;
            menu->add(
                path.c_str(), 0,
                [](Fl_Widget *, void *data) {
                    const SnesChoice &choice = *static_cast<SnesChoice *>(data);
                    choice.ui->release();
                    choice.ui->host.action(choice.ui->host.debugger, UI_SNES_PORT, choice.port,
                                           choice.device);
                },
                &snes_choices.back(),
                FL_MENU_RADIO | (snes.ports[port] == device ? FL_MENU_VALUE : 0) |
                    (available ? 0 : FL_MENU_INACTIVE));
        };
        option("Detached", DBG_SNES_DETACHED);
        option("Emulated SNES Mouse", DBG_SNES_MOUSE);
        for (int index = 0; index < DBG_HOST_GAMEPADS; ++index)
            if (snes.gamepads[index].available || snes.ports[port] == index)
                option(std::string(snes.gamepads[index].name) + " [" + std::to_string(index) + "]", index,
                       snes.gamepads[index].available);
    }
    add("SNES/Reset Mouse Speed");
}

bool dbg_ui_t::panel_open(int id) const
{
    if (workspace.leaf(id))
        return true;
    for (const Floating &f : workspace.floating)
        if (f.panel == id)
            return true;
    return false;
}

void dbg_ui_t::set_debugging(bool on)
{
    if (debugging == on)
        return;
    debugging = on;
    if (on) {
        if (debug_workspace_saved) {
            workspace = debug_workspace.clone();
            debug_workspace_saved = false;
        }
    } else {
        debug_workspace = workspace.clone();
        debug_workspace_saved = true;
        for (int id = 0; id < zeal_ui::PANEL_COUNT; id++)
            if (id != zeal_ui::PANEL_VIDEO)
                workspace.hide(id);
        // The video is what the window is for while the debugger is off, so a hidden
        // one has to come back even if the saved layout did not have it open.
        if (!workspace.leaf(zeal_ui::PANEL_VIDEO))
            workspace.show(zeal_ui::PANEL_VIDEO);
    }
    changed_layout();
}

void dbg_ui_t::detach_panel(int id)
{
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh, window->x(), window->y());
    int gap = 8, cascade = int(workspace.floating.size() % 5) * 24;
    workspace.detach(id, sx + sw / 2 + cascade, sy + gap + cascade);
    workspace.floating.back().w = std::min(640, sw / 2);
    workspace.floating.back().h = std::min(480, sh / 2);
    changed_layout();
}

bool dbg_ui_t::separate_windows() const
{
    // Separate-windows mode holds when the Video panel is the only docked one and at
    // least one other panel lives in its own window.
    if (workspace.floating.empty() || !workspace.leaf(0))
        return false;
    for (int id = 1; id < 8; id++)
        if (workspace.leaf(id))
            return false;
    return true;
}

void dbg_ui_t::set_separate_windows(bool on)
{
    separate_mode = on;
    unsigned hidden_mask = workspace.hidden;
    workspace.reset();
    for (int id = 0; id < 8; id++)
        if (hidden_mask & (1u << id))
            workspace.hide(id); // reset() drops the previous visibility mask
    if (on) {
        // Keep the Video panel (and the menu/toolbar) in the main window and tile the
        // other panels beside it. They must not overlap the main window, or its menu
        // bar becomes unreachable.
        if (!main_geometry_saved) {
            main_x = window->x();
            main_y = window->y();
            main_w = window->w();
            main_h = window->h();
            main_geometry_saved = true;
        }
        int sx, sy, sw, sh;
        Fl::screen_work_area(sx, sy, sw, sh, window->x(), window->y());
        int gap = 8;
        int left = std::clamp(sw * 2 / 5, 380, 720);
        int area_w = std::max(200, sw - left - 3 * gap);
        int area_h = std::max(200, sh - 2 * gap);
        window->resize(sx + gap, sy + gap, left, std::max(240, sh - 2 * gap));

        std::vector<int> visible;
        for (int id = 1; id < 8; id++)
            if (!(hidden_mask & (1u << id)))
                visible.push_back(id);
        int n = int(visible.size());
        // Pick the column count that gives the largest grid that still keeps panels usable.
        int cols = std::max(1, int(std::ceil(std::sqrt(double(n)))));
        for (int c = 1; c <= n; c++) {
            int r = (n + c - 1) / c;
            if ((area_w - gap * (c + 1)) / c >= 360 && (area_h - gap * (r + 1)) / r >= 260)
                cols = c;
        }
        int rows = std::max(1, (n + cols - 1) / cols);
        int cw = std::max(320, (area_w - gap * (cols + 1)) / cols);
        int ch = std::max(220, (area_h - gap * (rows + 1)) / rows);
        for (int i = 0; i < n; i++) {
            workspace.detach(visible[i], sx + left + 2 * gap + (i % cols) * (cw + gap),
                             sy + gap + (i / cols) * (ch + gap));
            workspace.floating.back().w = cw;
            workspace.floating.back().h = ch;
        }
    } else if (main_geometry_saved) {
        window->resize(main_x, main_y, main_w, main_h);
        main_geometry_saved = false;
    }
    changed_layout();
}

void dbg_ui_t::apply_icons()
{
    for (int i = 0; i < zeal_icons::ICON_COUNT; i++) {
        if (!icon_pixels[i]) {
            icon_pixels[i] = new unsigned char[zeal_icons::ICON_SIZE * zeal_icons::ICON_SIZE * 4];
            // The image only borrows the buffer, so dbg_ui_t keeps ownership of it.
            icons[i] = new Fl_RGB_Image(icon_pixels[i], zeal_icons::ICON_SIZE, zeal_icons::ICON_SIZE, 4);
        }
        zeal_icons::expand((zeal_icons::Icon)i, color("icon"), icon_pixels[i]);
        if (toolbar[i]) {
            toolbar[i]->image(icons[i]);
            toolbar[i]->redraw();
        }
    }
}

void dbg_ui_t::screenshot()
{
    // Ask the debugger for the frame directly so the capture does not depend on the
    // Video panel being visible or up to date.
    dbg_image_info_t info{};
    if (debugger_image_copy(host.debugger, -1, &info, nullptr, 0) != DBG_CAPACITY || !info.width) {
        message = "No video frame to capture";
        return;
    }
    std::vector<uint8_t> frame(info.stride * info.height);
    if (debugger_image_copy(host.debugger, -1, &info, frame.data(), frame.size()) != DBG_OK) {
        message = "No video frame to capture";
        return;
    }
    std::string path;
    for (int n = 1; n < 1000 && path.empty(); n++) {
        char name[64];
        std::snprintf(name, sizeof(name), "/zeal-screenshot-%d.bmp", n);
        if (!std::filesystem::exists(directory + name))
            path = directory + name;
    }
    if (path.empty()) {
        message = "Too many screenshots in " + directory;
        return;
    }
    int w = info.width, h = info.height, stride = (w * 3 + 3) & ~3, size = stride * h;
    std::ofstream f(path, std::ios::binary);
    unsigned char header[54] = {'B', 'M'};
    auto put32 = [&](int off, uint32_t v) {
        for (int i = 0; i < 4; i++)
            header[off + i] = (v >> (8 * i)) & 255;
    };
    put32(2, 54 + size);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    header[26] = 1;
    header[28] = 24;
    put32(34, size);
    f.write((const char *)header, sizeof(header));
    std::vector<unsigned char> line(stride, 0);
    for (int y = h - 1; y >= 0 && f; y--) { // BMP rows are stored bottom-up
        for (int x = 0; x < w; x++) {
            const uint8_t *src = &frame[y * info.stride + x * 4];
            line[x * 3] = src[2]; // BGR
            line[x * 3 + 1] = src[1];
            line[x * 3 + 2] = src[0];
        }
        f.write((const char *)line.data(), stride);
    }
    message = (f ? "Saved " : "Cannot write ") + path;
    last_refresh = 0;
}

void dbg_ui_t::layout_status()
{
    // Widths come from the widest value each field can hold, so labels can change
    // without the bar jittering. ST_MESSAGE stays flexible and absorbs the slack.
    static const char *samples[ST_COUNT] = {"Running", "PC $FFFF", "Frame 99999999", "59.97 fps", "",
                                            "Zeal 8-bit Computer"};
    int pad = theme.spacing;
    fl_font(ui_font, theme.font_size);
    for (int i = 0; i < ST_COUNT; i++)
        if (i != ST_MESSAGE)
            status_bar->fixed(status_fields[i], int(fl_width(samples[i])) + 2 * pad);
    status_bar->fixed(status_dot, theme.row_height);
}

class MainWindow : public Fl_Double_Window
{
    dbg_ui_t *ui;

  public:
    MainWindow(dbg_ui_t *u, int w, int h) : Fl_Double_Window(w, h, "Zeal 8-bit Debugger"), ui(u)
    {
    }
    int handle(int event) override
    {
        if (event == FL_UNFOCUS)
            ui->release();
        Panel *video = ui->panels[zeal_ui::PANEL_VIDEO];
        if (video && video->captures_input() &&
            (event == FL_PUSH || event == FL_DRAG || event == FL_MOVE || event == FL_RELEASE ||
             event == FL_MOUSEWHEEL))
            return video->forward_input(event);
        // With the debugger off this window is the only one the emulator has, so keys
        // that no widget wanted belong to the guest. Menu accelerators are handled by
        // the menu bar before the window sees them, so they are not swallowed here.
        if (!ui->debugging && (event == FL_KEYDOWN || event == FL_KEYUP || event == FL_SHORTCUT) &&
            !Fl::event_state(FL_COMMAND)) {
            unsigned key = fltk_key_to_host(Fl::event_key());
            if (key) {
                ui->host.key(ui->host.debugger, key, event != FL_KEYUP);
                return 1;
            }
        }
        return Fl_Double_Window::handle(event);
    }
};


namespace zeal_ui
{
// The one place that names the panels. Every panel id is listed exactly once, next to
// the class that implements it, so nothing else has to know the numbering.
struct PanelSpec {
    const char *name;
    Panel *(*create)(dbg_ui_t *);
};
const PanelSpec kPanels[PANEL_COUNT] = {
    {"Video", [](dbg_ui_t *u) -> Panel * { return new VideoPanel(u); }},
    {"CPU", [](dbg_ui_t *u) -> Panel * { return new CpuPanel(u); }},
    {"Breakpoints", [](dbg_ui_t *u) -> Panel * { return new BreakpointsPanel(u); }},
    {"Disassembler", [](dbg_ui_t *u) -> Panel * { return new DisassemblerPanel(u); }},
    {"Memory", [](dbg_ui_t *u) -> Panel * { return new MemoryPanel(u); }},
    {"MMU", [](dbg_ui_t *u) -> Panel * { return new MmuPanel(u); }},
    {"Semihost", [](dbg_ui_t *u) -> Panel * { return new SemihostPanel(u); }},
    {"VRAM", [](dbg_ui_t *u) -> Panel * { return new VramPanel(u); }},
};

const char *panel_name(int id)
{
    assert(id >= 0 && id < PANEL_COUNT);
    return kPanels[id].name;
}

Panel *create_panel(dbg_ui_t *ui, int id)
{
    assert(id >= 0 && id < PANEL_COUNT);
    return kPanels[id].create(ui);
}

int create_debugger_ui(dbg_ui_t **out, const dbg_ui_init_args_t *args)
{
    if (!out || !args || !args->debugger || !args->config_directory)
        return -1;
    dbg_ui_t *u = new dbg_ui_t;
    u->host = *args;
    u->directory = args->config_directory;
    u->volume = args->volume;
    u->passthrough = args->passthrough;
    Fl::scheme("gtk+");
    int width = args->width > 600 ? args->width : 1280, height = args->height > 400 ? args->height : 900;
    std::ifstream pref(u->directory + "/fltk-preferences.ini");
    std::string line;
    while (std::getline(pref, line)) {
        size_t eq = line.find('=');
        if (eq == line.npos)
            continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "theme" && value.find('/') == value.npos && value.find('\\') == value.npos)
            u->theme_name = value;
        try {
            if (key == "width")
                width = std::clamp(std::stoi(value), 640, 4000);
            if (key == "height")
                height = std::clamp(std::stoi(value), 480, 3000);
            if (key == "upper")
                u->upper = std::stoi(value) != 0;
            if (key == "cp437")
                u->cp437 = std::stoi(value) != 0;
        } catch (...) {
        }
    }
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh);
    width = std::min(width, sw);
    height = std::min(height, sh);
    u->window = new MainWindow(u, width, height);
    // Position alone can leave FLTK's cached screen at 0, so menus clamp to
    // the wrong monitor even though Cocoa places the owner window correctly.
    u->window->screen_num(Fl::screen_num(sx, sy, sw, sh));
    u->window->position(sx + (sw - width) / 2, sy + (sh - height) / 2);
    u->window->size_range(640, 480);
    u->window->begin();
    // One vertical Fl_Flex owns the window chrome: menu, toolbar, workspace, status bar.
    // The workspace takes the slack, so nothing depends on hardcoded pixel offsets.
    u->shell = new Fl_Flex(0, 0, width, height, Fl_Flex::VERTICAL);
    u->shell->gap(u->theme.spacing);
    u->shell->begin();
    u->menu = new Fl_Menu_Bar(0, 0, width, u->theme.row_height + 4);
    u->shell->fixed(u->menu, u->theme.row_height + 4);
    u->build_menu();
    u->toolbar_row = new Fl_Flex(0, 0, width, u->theme.row_height, Fl_Flex::HORIZONTAL);
    u->toolbar_row->gap(u->theme.spacing);
    // Fl_Widget::argument() and user_data() share one storage slot, so a button can
    // hold either its command or its context, never both. The buttons carry the
    // command; the context comes from this row.
    u->toolbar_row->user_data(u);
    u->toolbar_row->begin();
    const dbg_command_t commands[TB_BREAKPOINT] = {DBG_CONTINUE, DBG_PAUSE, DBG_STEP, DBG_STEP_OVER,
                                                   DBG_RESET};
    const char *tips[TB_COUNT] = {"Continue", "Pause", "Step into", "Step over", "Reset",
                                  "Toggle a breakpoint at the current PC",
                                  "Save a BMP of the emulated screen"};
    for (int i = 0; i < TB_COUNT; i++) {
        Fl_Button *b = new Fl_Button(0, 0, u->theme.row_height * 4 / 3, u->theme.row_height);
        u->toolbar[i] = b;
        u->toolbar_row->fixed(b, u->theme.row_height * 4 / 3);
        b->tooltip(tips[i]);
        if (i < TB_BREAKPOINT) {
            b->argument(commands[i]); // the callback's data, and user_data() at the same time
            b->callback([](Fl_Widget *w, void *data) {
                dbg_ui_t *u = (dbg_ui_t *)w->parent()->user_data();
                u->command((dbg_command_t)(intptr_t)data);
            });
        } else if (i == TB_BREAKPOINT) {
            b->callback([](Fl_Widget *w, void *) {
                dbg_ui_t *u = (dbg_ui_t *)w->parent()->user_data();
                debugger_toggle_breakpoint(u->host.debugger, u->snapshot.pc);
                u->message = "Toggled breakpoint at $" + hex(u->snapshot.pc, 4);
                u->last_refresh = 0;
            });
        } else {
            b->callback([](Fl_Widget *w, void *) {
                dbg_ui_t *u = (dbg_ui_t *)w->parent()->user_data();
                u->screenshot();
            });
        }
    }
    new Fl_Box(0, 0, 1, u->theme.row_height); // slack absorber keeps buttons left-aligned
    u->toolbar_row->end();
    u->shell->fixed(u->toolbar_row, u->theme.row_height);
    u->window->user_data(u);
    u->surface = new DockSurface(u, 0, 0, width, height);
    // A segmented status bar: fixed-width fields joined by 1px dividers, with the
    // notification field absorbing the slack and the machine name pinned right.
    u->status_bar = new Fl_Flex(0, 0, width, u->theme.row_height, Fl_Flex::HORIZONTAL);
    u->status_bar->gap(0);
    u->status_bar->begin();
    for (int i = 0; i < ST_COUNT; i++) {
        if (i) {
            Fl_Box *sep = new Fl_Box(0, 0, 1, u->theme.row_height);
            sep->box(FL_FLAT_BOX);
            u->status_bar->fixed(sep, 1);
            u->status_separators.push_back(sep);
        }
        Fl_Box *f = new Fl_Box(0, 0, 0, u->theme.row_height);
        f->align(i == ST_MACHINE ? (FL_ALIGN_RIGHT | FL_ALIGN_INSIDE) : (FL_ALIGN_LEFT | FL_ALIGN_INSIDE));
        u->status_fields[i] = f;
    }
    u->status_dot = new Fl_Box(0, 0, u->theme.row_height, u->theme.row_height, "●");
    u->status_dot->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    u->status_bar->fixed(u->status_dot, u->theme.row_height);
    u->status_bar->end();
    u->shell->fixed(u->status_bar, u->theme.row_height);
    u->shell->end();
    u->window->resizable(u->shell);
    u->window->end();
    install_input_menu(u);
    u->window->callback(
        [](Fl_Widget *, void *data) {
            dbg_ui_t *u = (dbg_ui_t *)data;
            u->save();
            u->command(DBG_STOP);
        },
        u);
    Fl_Group::current(nullptr);
    for (int i = 0; i < 8; i++)
        u->panels[i] = zeal_ui::create_panel(u, i);
    if (!u->workspace.load(u->directory + "/fltk-workspace.ini"))
        for (int i = 0; i < 8; i++)
            if (args->hidden_panels & (1u << i))
                u->workspace.hide(i);
    u->glyph_font.load(u->host, s_cp437_to_unicode);
    u->separate_mode = u->separate_windows();
    u->layout();
    std::string initial_theme = u->theme_name;
    u->theme_name = "Dark";
    u->select_theme(initial_theme);
    *out = u;
    return 0;
}

void destroy_debugger_ui(dbg_ui_t *u)
{
    if (!u)
        return;
    u->save();
    u->release();
    // Hide every window before deleting any widget. FLTK keeps global pointers to the
    // focused window and to the window under the mouse, and hiding a window is what
    // releases them. Deleting widgets first leaves those pointers aimed at freed memory,
    // and the next widget destruction walks them looking for a new focus target.
    for (auto [id, w] : u->floating)
        w->hide();
    u->window->hide();
    Fl::flush();
    for (Panel *p : u->panels) {
        if (p->parent())
            p->parent()->remove(p);
        delete p;
    }
    for (auto [id, w] : u->floating)
        delete w;
    delete u->window;
    shared_input_menu() = InputMenu{}; // the popup was a child of that window
    for (unsigned char *p : u->icon_pixels)
        delete[] p;
    delete u;
}
} // namespace zeal_ui
void dbg_ui_t::show_window(bool visible)
{
    shown = visible;
    release();
    if (visible)
        window->show();
    else
        window->hide();
    for (auto [id, w] : floating) {
        if (visible)
            w->show();
        else
            w->hide();
    }
}
