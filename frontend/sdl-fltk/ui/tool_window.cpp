/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Floating tool windows with self-registering views (see tool_window.h). */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <FL/fl_draw.H>
#include "tool_window.h"

namespace zui {

constexpr int TOOL_BAR_H = 38;

struct ViewInfo {
    int order;
    std::string name;
    ToolViewFactory factory;
};

struct WindowDef {
    const char* id;
    const char* title;
    const char* key; /* config key prefix */
};

static const WindowDef s_defs[] = {
    { TOOL_CODE, FZ_WIN_NAME " - Code & Profiler", "FLTK_TOOL_CODE" },
    { TOOL_HARDWARE, FZ_WIN_NAME " - Hardware", "FLTK_TOOL_HW" },
};

static std::map<std::string, std::vector<ViewInfo>>& registry()
{
    static std::map<std::string, std::vector<ViewInfo>> views;
    return views;
}

ToolViewRegistrar::ToolViewRegistrar(const char* window, int order, const char* name, ToolViewFactory factory)
{
    registry()[window].push_back({ order, name, std::move(factory) });
}

/* ------------------------------------------------------------------ */

class ToolTab : public Fl_Button {
public:
    ToolTab(int X, int Y, int W, int H, const char* label) : Fl_Button(X, Y, W, H, label)
    {
        type(FL_RADIO_BUTTON);
        box(FL_FLAT_BOX);
        clear_visible_focus();
    }

    void draw() override
    {
        const bool on = value() != 0;
        const bool hover = Fl::belowmouse() == this;
        fl_color(on ? COLOR_RAISED : hover ? COLOR_HOVER : COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        if (on) {
            fl_color(COLOR_AMBER);
            fl_rectf(x(), y() + h() - 2, w(), 2);
        }
        fl_font(FL_HELVETICA, 12);
        fl_color(on ? COLOR_TEXT : COLOR_DIM);
        fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
    }

    int handle(int event) override
    {
        if (event == FL_ENTER || event == FL_LEAVE) {
            redraw();
            return 1;
        }
        return Fl_Button::handle(event);
    }
};

class ToolWindow : public Fl_Double_Window {
public:
    ToolWindow(const WindowDef& def) : Fl_Double_Window(900, 600, def.title), m_def(def)
    {
        color(COLOR_BG);
        std::vector<ViewInfo> infos = registry()[def.id];
        std::stable_sort(infos.begin(), infos.end(),
                         [](const ViewInfo& a, const ViewInfo& b) { return a.order < b.order; });
        fl_font(FL_HELVETICA, 12);
        m_names.reserve(infos.size()); /* tab labels point into m_names */
        int tx = 8;
        for (size_t i = 0; i < infos.size(); i++) {
            const int tw = (int) fl_width(infos[i].name.c_str()) + 28;
            m_names.push_back(infos[i].name);
            ToolTab* tab = new ToolTab(tx, 6, tw, TOOL_BAR_H - 10, m_names.back().c_str());
            tab->callback(cb_tab, (void*) (intptr_t) i);
            m_tabs.push_back(tab);
            tx += tw + 2;
        }
        for (const ViewInfo& info : infos) {
            Panel* view = info.factory(0, TOOL_BAR_H, w(), h() - TOOL_BAR_H);
            if (view->parent() != this) add(view);
            view->hide();
            m_views.push_back(view);
        }
        if (m_views.empty()) {
            Fl_Box* none = new Fl_Box(0, TOOL_BAR_H, w(), h() - TOOL_BAR_H, "No views yet");
            none->labelcolor(COLOR_DIM);
        }
        end();
        size_range(420, 260);
        set_non_modal();
        callback(cb_close, this);
        load();
        select(m_current);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Double_Window::resize(X, Y, W, H);
        for (Panel* v : m_views) v->resize(0, TOOL_BAR_H, W, H - TOOL_BAR_H);
    }

    void draw() override
    {
        Fl_Double_Window::draw();
        fl_color(COLOR_BORDER);
        fl_xyline(0, TOOL_BAR_H - 1, w());
    }

    void select(int index)
    {
        if (m_views.empty()) return;
        if (index < 0 || index >= (int) m_views.size()) index = 0;
        m_current = index;
        for (size_t i = 0; i < m_views.size(); i++) {
            if ((int) i == index) m_views[i]->show(); else m_views[i]->hide();
        }
        m_tabs[index]->setonly();
        refresh(fz_paused(), true);
        redraw();
    }

    void refresh(bool paused, bool force)
    {
        if (!shown() || m_views.empty()) return;
        const double now = now_seconds();
        if (!force && (paused || now - m_last < 0.25)) return;
        m_last = now;
        m_views[m_current]->refresh(paused, force);
    }

    void open()
    {
        m_wanted = true;
        show();
        refresh(fz_paused(), true);
    }

    void close()
    {
        capture();
        m_wanted = false;
        hide();
    }

    void suspend()
    {
        capture();
        hide();
    }

    bool wanted() const { return m_wanted; }

    void capture()
    {
        if (!shown()) return;
        m_x = x();
        m_y = y();
        m_w = w();
        m_h = h();
    }

    void reset()
    {
        m_x = m_y = -1;
        m_w = m_h = 0;
        m_wanted = false;
    }

    void save(rini_config* ini)
    {
        capture();
        char key[64];
        rini_set_config_comment_line(ini, m_def.title);
        snprintf(key, sizeof(key), "%s_X", m_def.key);
        rini_set_config_value(ini, key, m_x, "X");
        snprintf(key, sizeof(key), "%s_Y", m_def.key);
        rini_set_config_value(ini, key, m_y, "Y");
        snprintf(key, sizeof(key), "%s_WIDTH", m_def.key);
        rini_set_config_value(ini, key, m_w, "Width");
        snprintf(key, sizeof(key), "%s_HEIGHT", m_def.key);
        rini_set_config_value(ini, key, m_h, "Height");
        snprintf(key, sizeof(key), "%s_SHOWN", m_def.key);
        rini_set_config_value(ini, key, m_wanted, "Open");
        snprintf(key, sizeof(key), "%s_VIEW", m_def.key);
        rini_set_config_value(ini, key, m_current, "Selected view");
    }

private:
    void load()
    {
        char key[64];
        snprintf(key, sizeof(key), "%s_X", m_def.key);
        m_x = config_get(key, -1);
        snprintf(key, sizeof(key), "%s_Y", m_def.key);
        m_y = config_get(key, -1);
        snprintf(key, sizeof(key), "%s_WIDTH", m_def.key);
        m_w = config_get(key, 0);
        snprintf(key, sizeof(key), "%s_HEIGHT", m_def.key);
        m_h = config_get(key, 0);
        snprintf(key, sizeof(key), "%s_VIEW", m_def.key);
        m_current = config_get(key, 0);
        if (m_w > 200 && m_h > 150) {
            resize(m_x >= 0 ? m_x : x(), m_y >= 0 ? m_y : y(), m_w, m_h);
        }
    }

    static void cb_tab(Fl_Widget* w, void* data)
    {
        static_cast<ToolWindow*>(w->window())->select((int) (intptr_t) data);
    }

    static void cb_close(Fl_Widget*, void* data)
    {
        static_cast<ToolWindow*>(data)->close();
        if (main_window()) main_window()->update_menus();
    }

    const WindowDef& m_def;
    std::vector<std::string> m_names;
    std::vector<ToolTab*> m_tabs;
    std::vector<Panel*> m_views;
    int m_current = 0;
    double m_last = 0;
    bool m_wanted = false;
    int m_x = -1, m_y = -1, m_w = 0, m_h = 0;
};

/* ------------------------------------------------------------------ */

static std::map<std::string, ToolWindow*>& windows()
{
    static std::map<std::string, ToolWindow*> map;
    return map;
}

static bool s_persist_registered;

static const WindowDef* find_def(const char* id)
{
    for (const WindowDef& d : s_defs) {
        if (strcmp(d.id, id) == 0) return &d;
    }
    return nullptr;
}

static ToolWindow* get(const char* id, bool create)
{
    auto it = windows().find(id);
    if (it != windows().end()) return it->second;
    if (!create) return nullptr;
    const WindowDef* def = find_def(id);
    if (def == nullptr) return nullptr;
    ToolWindow* w = new ToolWindow(*def);
    windows()[id] = w;
    return w;
}

static void persist_once()
{
    if (s_persist_registered) return;
    s_persist_registered = true;
    persist_register([](rini_config* ini) {
        for (const WindowDef& d : s_defs) {
            ToolWindow* w = get(d.id, false);
            if (w) {
                w->save(ini);
            } else {
                /* Never opened this session: keep what was loaded */
                char key[64];
                const char* fields[] = { "_X", "_Y", "_WIDTH", "_HEIGHT", "_SHOWN", "_VIEW" };
                const int defaults[] = { -1, -1, 0, 0, 0, 0 };
                for (int i = 0; i < 6; i++) {
                    snprintf(key, sizeof(key), "%s%s", d.key, fields[i]);
                    rini_set_config_value(ini, key, config_get(key, defaults[i]), "");
                }
            }
        }
    });
}

void tool_window_show(const char* id, bool show)
{
    persist_once();
    ToolWindow* w = get(id, show);
    if (w == nullptr) return;
    if (show) w->open(); else w->close();
}

bool tool_window_shown(const char* id)
{
    ToolWindow* w = get(id, false);
    return w && w->shown();
}

void tool_windows_refresh(bool paused, bool force)
{
    for (auto& entry : windows()) entry.second->refresh(paused, force);
}

void tool_windows_suspend()
{
    for (auto& entry : windows()) entry.second->suspend();
}

void tool_windows_resume()
{
    persist_once();
    for (const WindowDef& d : s_defs) {
        ToolWindow* w = get(d.id, false);
        char key[64];
        snprintf(key, sizeof(key), "%s_SHOWN", d.key);
        const bool wanted = w ? w->wanted() : config_get(key, 0) != 0;
        if (wanted) tool_window_show(d.id, true);
    }
}

void tool_windows_destroy()
{
    for (auto& entry : windows()) {
        entry.second->capture();
        entry.second->hide();
    }
    /* Keep the objects alive until exit so their state can still be saved */
}

void tool_windows_reset()
{
    for (auto& entry : windows()) {
        entry.second->hide();
        entry.second->reset();
    }
}

} // namespace zui
