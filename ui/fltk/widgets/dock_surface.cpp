// SPDX-License-Identifier: Apache-2.0
#include "ui/fltk/widgets/dock_surface.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/panel.h"
#include "ui/fltk/util.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <algorithm>

using zeal_ui::text;


using zeal_ui::DockNode;

DockSurface::DockSurface(dbg_ui_t *u, int x, int y, int w, int h) : Fl_Group(x, y, w, h), ui(u)
{
    end();
}

void DockSurface::arrange(DockNode *n, Rect r)
{
    if (!n)
        return;
    if (n->axis) {
        int size = n->axis == 1 ? r.w : r.h;
        int cut =
            std::clamp(int(size * n->ratio), std::min(100, size / 2), std::max(size / 2, size - 100));
        Rect a = r, b = r, s = r;
        if (n->axis == 1) {
            a.w = std::max(1, cut - 3);
            b.x += cut + 3;
            b.w = std::max(1, r.w - cut - 3);
            s.x += cut - 3;
            s.w = 6;
        } else {
            a.h = std::max(1, cut - 3);
            b.y += cut + 3;
            b.h = std::max(1, r.h - cut - 3);
            s.y += cut - 3;
            s.h = 6;
        }
        splits.push_back({n, s, r});
        arrange(n->first.get(), a);
        arrange(n->second.get(), b);
    } else {
        leaves.push_back({n, r});
        // The classic view has no tabs to draw or drag, so the panel gets the whole leaf.
        int head = ui->debugging ? ui->theme.row_height : 0;
        for (size_t i = 0; i < n->tabs.size(); i++) {
            Panel *p = ui->panels[n->tabs[i]];
            if (p->parent() != this)
                add(p);
            p->resize(r.x, r.y + head, r.w, std::max(1, r.h - head));
            if (int(i) == n->selected)
                p->show();
            else
                p->hide();
        }
    }
}

void DockSurface::update()
{
    leaves.clear();
    splits.clear();
    arrange(ui->workspace.root.get(), {x(), y(), w(), h()});
    redraw();
}

void DockSurface::resize(int x, int y, int w, int h)
{
    Fl_Group::resize(x, y, w, h);
    update();
}

void DockSurface::draw()
{
    fl_push_clip(x(), y(), w(), h());
    // Repaint the dock background only on a full redraw. Filling it unconditionally
    // paints over children that repaint themselves only partially: a focused
    // Fl_Input blinking its cursor would be left showing the dock background with
    // no text. Fl_Group::draw_children() only redraws children that are damaged.
    if (damage() & ~FL_DAMAGE_CHILD) {
        fl_color(ui->color("background"));
        fl_rectf(x(), y(), w(), h());
    }
    draw_children();
    if (!ui->debugging) {
        fl_pop_clip();
        return;
    }
    int head = ui->theme.row_height;
    for (DockSurface::Leaf l : leaves) {
        fl_color(ui->color("border"));
        fl_rect(l.rect.x, l.rect.y, l.rect.w, l.rect.h);
        int count = l.node->tabs.size(), width = std::max(32, (l.rect.w - 24) / count);
        for (int i = 0; i < count; i++) {
            fl_color(ui->color(i == l.node->selected ? "selection" : "background"));
            fl_rectf(l.rect.x + i * width, l.rect.y, width, head);
            text(ui, "text", zeal_ui::panel_name(l.node->tabs[i]), l.rect.x + i * width + 6,
                 l.rect.y + head - 6, false);
        }
        text(ui, "muted", "×", l.rect.x + l.rect.w - 18, l.rect.y + head - 6, false);
    }
    if (ui->drag >= 0) {
        int mx = Fl::event_x_root() - window()->x(), my = Fl::event_y_root() - window()->y();
        for (DockSurface::Leaf l : leaves)
            if (l.rect.contains(mx, my)) {
                Rect r = l.rect;
                int edge = target_edge(r, mx, my);
                if (edge == 1)
                    r.w /= 3;
                else if (edge == 2) {
                    r.x += r.w * 2 / 3;
                    r.w /= 3;
                } else if (edge == 3)
                    r.h /= 3;
                else if (edge == 4) {
                    r.y += r.h * 2 / 3;
                    r.h /= 3;
                }
                fl_color(ui->color("link"));
                fl_line_style(FL_SOLID, 3);
                fl_rect(r.x + 2, r.y + 2, r.w - 4, r.h - 4);
                fl_line_style(0);
            }
    }
    fl_pop_clip();
}

int DockSurface::target_edge(Rect r, int x, int y)
{
    if (x < r.x + r.w / 4)
        return 1;
    if (x > r.x + r.w * 3 / 4)
        return 2;
    if (y < r.y + r.h / 4)
        return 3;
    if (y > r.y + r.h * 3 / 4)
        return 4;
    return 0;
}

int DockSurface::handle(int event)
{
    // Nothing here can be rearranged while the debugger is off: the events belong to
    // the video panel that fills the surface.
    if (!ui->debugging) {
        return Fl_Group::handle(event);
    }
    int mx = Fl::event_x(), my = Fl::event_y();
    if (event == FL_PUSH) {
        for (DockSurface::Split s : splits)
            if (s.rect.contains(mx, my)) {
                resizing = s.node;
                resize_rect = s.full;
                return 1;
            }
        for (DockSurface::Leaf l : leaves)
            if (Rect{l.rect.x, l.rect.y, l.rect.w, ui->theme.row_height}.contains(mx, my)) {
                int id = l.node->tabs[l.node->selected];
                if (mx > l.rect.x + l.rect.w - 24) {
                    ui->hide_panel(id);
                    return 1;
                }
                int width = std::max(32, (l.rect.w - 24) / (int)l.node->tabs.size());
                int index = std::min(int(l.node->tabs.size()) - 1, (mx - l.rect.x) / width);
                l.node->selected = index;
                id = l.node->tabs[index];
                ui->drag = id;
                ui->drag_x = Fl::event_x_root();
                ui->drag_y = Fl::event_y_root();
                update();
                return 1;
            }
    }
    if (event == FL_DRAG) {
        if (resizing) {
            resizing->ratio = std::clamp(resizing->axis == 1 ? double(mx - resize_rect.x) / resize_rect.w
                                                             : double(my - resize_rect.y) / resize_rect.h,
                                         .1, .9);
            update();
            return 1;
        }
        if (ui->drag >= 0) {
            redraw();
            return 1;
        }
    }
    if (event == FL_RELEASE) {
        if (resizing) {
            resizing = nullptr;
            return 1;
        }
        if (ui->drag >= 0) {
            if (std::abs(Fl::event_x_root() - ui->drag_x) + std::abs(Fl::event_y_root() - ui->drag_y) > 8)
                ui->drop(Fl::event_x_root(), Fl::event_y_root());
            ui->drag = -1;
            redraw();
            return 1;
        }
    }
    return Fl_Group::handle(event);
}
