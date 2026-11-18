/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Symbols browser (Code & Profiler window): every symbol loaded from map
 * files (-m on the command line, or opened here), filterable and sortable by
 * name or address. */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/filename.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include "tool_window.h"
#include "frontend/sdl-fltk/bridge/symbols.h"
#include "utils/notif.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

namespace {

constexpr int COLS_H = 22;   /* column header row */
constexpr int GUTTER = 22;   /* breakpoint marker column */
constexpr int SCROLL_W = 14;

struct Symbol {
    std::string name;
    std::string lower;  /* for case-insensitive filtering */
    hwaddr addr;
    uint8_t kind;       /* symbol_kind_t */
    bool local;
};

/* Kind filter choices, in menu order: All, then each symbol_kind_t */
constexpr int KIND_ALL = -1;

const char* kind_label(uint8_t kind)
{
    switch (kind) {
        case SYM_KIND_CODE: return "CODE";
        case SYM_KIND_DATA: return "DATA";
        case SYM_KIND_CONST: return "CONST";
        default: return "OTHER";
    }
}

Fl_Color kind_color(uint8_t kind)
{
    switch (kind) {
        case SYM_KIND_CODE: return COLOR_GREEN;
        case SYM_KIND_DATA: return COLOR_CYAN;
        case SYM_KIND_CONST: return COLOR_TEXT;
        default: return COLOR_DIM;
    }
}

/** Code opens in the disassembly, data in the memory editor. */
bool is_data(uint8_t kind)
{
    return kind == SYM_KIND_DATA || kind == SYM_KIND_CONST;
}

std::string to_lower(const char* s)
{
    std::string out(s ? s : "");
    for (char& c : out) c = (char) tolower((unsigned char) c);
    return out;
}

/** Address in hex, honouring the Hex Upper preference. */
void format_addr(char* out, size_t size, hwaddr addr)
{
    if (addr <= 0xFFFF) hex16(out, size, addr);
    else snprintf(out, size, config.debugger.hex_upper ? "%06X" : "%06x", (unsigned) addr);
}

/** Filter field that clears itself on Escape. */
class FilterInput : public Fl_Input {
public:
    FilterInput(int X, int Y, int W, int H) : Fl_Input(X, Y, W, H) {}
    int handle(int event) override
    {
        if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape && size() > 0) {
            value("");
            do_callback();
            return 1;
        }
        return Fl_Input::handle(event);
    }
};

class SymbolList : public Fl_Widget {
public:
    enum class Sort { Name, Kind, Address };

    SymbolList(int X, int Y, int W, int H, Fl_Scrollbar* bar) : Fl_Widget(X, Y, W, H), m_bar(bar) {}

    /** Reload from the debugger if the symbol table changed. */
    bool sync()
    {
        dbg_t* dbg = fz_debugger();
        size_t count = 0;
        uintptr_t sig = 0;
        for (const symbols_t* b = dbg ? &dbg->symbols : nullptr; b; b = b->next) {
            for (unsigned i = 0; i < b->count; i++) {
                sig = sig * 31 + (uintptr_t) b->array[i].name + b->array[i].addr + b->array[i].kind;
                count++;
            }
        }
        if (count == m_all.size() && sig == m_sig) return false;
        m_sig = sig;
        m_all.clear();
        m_all.reserve(count);
        for (const symbols_t* b = dbg ? &dbg->symbols : nullptr; b; b = b->next) {
            for (unsigned i = 0; i < b->count; i++) {
                const symbol_t& s = b->array[i];
                if (s.name == nullptr) continue;
                m_all.push_back({ s.name, to_lower(s.name), s.addr, s.kind, s.local });
            }
        }
        apply();
        return true;
    }

    void filter(const char* text)
    {
        m_filter = to_lower(text);
        m_top = 0;
        apply();
    }

    /** Show only one symbol_kind_t, or KIND_ALL. */
    void filter_kind(int kind)
    {
        m_kind = kind;
        m_top = 0;
        apply();
    }

    /** Sort by `key`; picking the current key again flips the direction. */
    void sort_by(Sort key)
    {
        if (key == m_sort) m_ascending = !m_ascending;
        else {
            m_sort = key;
            m_ascending = true;
        }
        apply();
    }

    size_t total() const { return m_all.size(); }
    size_t shown() const { return m_rows.size(); }
    bool filtered() const { return !m_filter.empty() || m_kind != KIND_ALL; }

    void scroll_to(int row)
    {
        const int max_top = std::max(0, (int) m_rows.size() - visible_rows());
        m_top = std::max(0, std::min(row, max_top));
        sync_bar();
        redraw();
    }

    void sync_bar()
    {
        const int vis = visible_rows();
        m_bar->value(m_top, vis, 0, std::max((int) m_rows.size(), vis));
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        scroll_to(m_top);
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        fl_push_clip(x(), y(), w(), h());
        draw_columns();

        const int lh = row_h();
        const int asc = text_baseline(lh);
        const int ry0 = y() + COLS_H;
        if (m_rows.empty()) {
            fl_font(FL_HELVETICA, 12);
            fl_color(COLOR_DIM);
            const char* msg = m_all.empty()
                ? "No symbols loaded.\nClick Open Map... to load your program's map file\n(z88dk or SDCC; or start with -m program.map)."
                : "No symbol matches the filter.\nPress Escape in the filter field to clear it, or show all kinds.";
            fl_draw(msg, x() + 12, ry0 + 10, w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
            fl_pop_clip();
            return;
        }

        dbg_t* dbg = fz_debugger();
        const int kx = kind_x();
        const int ax = addr_x();
        char text[16];
        fl_font(DATA_FONT, DATA_SIZE);
        const int end = std::min((int) m_rows.size(), m_top + visible_rows() + 1);
        for (int r = m_top; r < end; r++) {
            const Symbol& s = m_all[(size_t) m_rows[(size_t) r]];
            const int ry = ry0 + (r - m_top) * lh;
            if (r == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
            }
            if (dbg && debugger_has_breakpoint(dbg, s.addr)) {
                const bool on = debugger_is_breakpoint_enabled(dbg, s.addr);
                fl_color(COLOR_BREAK);
                const int d = 9;
                const int bx = x() + (GUTTER - d) / 2, by = ry + (lh - d) / 2;
                if (on) {
                    fl_pie(bx, by, d, d, 0, 360);
                } else {
                    fl_arc(bx, by, d, d, 0, 360);
                }
            }
            fl_font(DATA_FONT, DATA_SIZE);
            /* File-local labels (name@label) are less prominent */
            fl_color(s.local ? COLOR_DIM : COLOR_AMBER);
            fl_push_clip(x() + GUTTER, ry, kx - x() - GUTTER - 8, lh);
            fl_draw(s.name.c_str(), x() + GUTTER, ry + asc);
            fl_pop_clip();
            fl_color(kind_color(s.kind));
            fl_draw(kind_label(s.kind), kx, ry + asc);
            fl_font(DATA_FONT, DATA_SIZE);
            format_addr(text, sizeof(text), s.addr);
            fl_color(COLOR_CYAN);
            fl_draw(text, ax, ry + asc);
        }
        fl_pop_clip();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                set_hover(-1);
                return 1;
            case FL_MOVE:
                set_hover(row_at(Fl::event_y()));
                return 1;
            case FL_MOUSEWHEEL:
                scroll_to(m_top + Fl::event_dy() * 3);
                set_hover(row_at(Fl::event_y()));
                return 1;
            case FL_PUSH: {
                const int ey = Fl::event_y();
                if (ey < y() + COLS_H) {
                    if (Fl::event_button() == FL_LEFT_MOUSE) {
                        const int ex = Fl::event_x();
                        sort_by(ex >= addr_x() - 8 ? Sort::Address : ex >= kind_x() - 8 ? Sort::Kind : Sort::Name);
                        if (m_changed) m_changed();
                    }
                    return 1;
                }
                const int r = row_at(ey);
                if (r < 0) return 1;
                const Symbol& s = m_all[(size_t) m_rows[(size_t) r]];
                if (Fl::event_button() == FL_RIGHT_MOUSE) context_menu(s);
                else if (Fl::event_button() == FL_LEFT_MOUSE) {
                    if (is_data(s.kind)) goto_memory(s.addr);
                    else goto_disassembly(s.addr);
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

    void on_change(std::function<void()> cb) { m_changed = std::move(cb); }

private:
    int row_h() const { return data_line_height(); }
    int visible_rows() const { return std::max(1, (h() - COLS_H) / row_h()); }

    int row_at(int ey) const
    {
        if (ey < y() + COLS_H) return -1;
        const int r = m_top + (ey - y() - COLS_H) / row_h();
        return r < (int) m_rows.size() ? r : -1;
    }

    void set_hover(int r)
    {
        if (r == m_hover) return;
        m_hover = r;
        redraw();
    }

    /** Kind column follows the longest shown name, within the widget. */
    int kind_x() const
    {
        const int cw = char_width();
        const int tail_w = cw * (KIND_CHARS + 6);
        int kx = x() + GUTTER + cw * (int) std::max<size_t>(m_name_chars, 8) + cw * 3;
        const int max_x = x() + w() - tail_w - 12;
        return std::max(x() + GUTTER + cw * 8, std::min(kx, max_x));
    }

    int addr_x() const { return kind_x() + char_width() * KIND_CHARS; }

    static constexpr int KIND_CHARS = 8; /* "CONST" plus a gap */

    void draw_columns()
    {
        fl_color(COLOR_RAISED);
        fl_rectf(x(), y(), w(), COLS_H);
        fl_color(COLOR_BORDER);
        fl_xyline(x(), y() + COLS_H - 1, x() + w());
        /* Column positions measure the data font: get them first */
        const int kx = kind_x(), ax = addr_x();
        fl_font(FL_HELVETICA, 11);
        draw_column("NAME", x() + GUTTER, m_sort == Sort::Name);
        draw_column("KIND", kx, m_sort == Sort::Kind);
        draw_column("ADDRESS (HEX)", ax, m_sort == Sort::Address);
    }

    void draw_column(const char* label, int cx, bool active)
    {
        const int ty = y() + (COLS_H + (int) fl_height()) / 2 - fl_descent();
        fl_color(active ? COLOR_TEXT : COLOR_DIM);
        fl_draw(label, cx, ty);
        if (!active) return;
        /* Sort direction arrow */
        const int ax = cx + (int) fl_width(label) + 6;
        const int ay = y() + COLS_H / 2;
        fl_color(COLOR_AMBER);
        if (m_ascending) fl_polygon(ax, ay + 2, ax + 8, ay + 2, ax + 4, ay - 3);
        else fl_polygon(ax, ay - 2, ax + 8, ay - 2, ax + 4, ay + 3);
    }

    void apply()
    {
        m_rows.clear();
        m_name_chars = 0;
        for (size_t i = 0; i < m_all.size(); i++) {
            if (!m_filter.empty() && m_all[i].lower.find(m_filter) == std::string::npos) continue;
            if (m_kind != KIND_ALL && m_all[i].kind != m_kind) continue;
            m_rows.push_back((int) i);
            m_name_chars = std::max(m_name_chars, m_all[i].name.size());
        }
        const std::vector<Symbol>& all = m_all;
        const bool asc = m_ascending;
        if (m_sort == Sort::Kind) {
            std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
                const int k1 = all[(size_t) a].kind, k2 = all[(size_t) b].kind;
                if (k1 != k2) return asc ? k1 < k2 : k1 > k2;
                return all[(size_t) a].addr < all[(size_t) b].addr;
            });
        } else if (m_sort == Sort::Name) {
            std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
                const int c = all[(size_t) a].lower.compare(all[(size_t) b].lower);
                if (c != 0) return asc ? c < 0 : c > 0;
                return all[(size_t) a].addr < all[(size_t) b].addr;
            });
        } else {
            std::stable_sort(m_rows.begin(), m_rows.end(), [&](int a, int b) {
                const hwaddr x1 = all[(size_t) a].addr, x2 = all[(size_t) b].addr;
                if (x1 != x2) return asc ? x1 < x2 : x1 > x2;
                return all[(size_t) a].lower < all[(size_t) b].lower;
            });
        }
        m_hover = -1;
        scroll_to(m_top);
    }

    void context_menu(const Symbol& s)
    {
        dbg_t* dbg = fz_debugger();
        Fl_Menu_Item items[] = {
            { "Show in Disassembly", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Show in Memory", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Toggle Breakpoint", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Copy Name", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Copy Address", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        char text[16];
        switch ((int) (picked - items)) {
            case 0: goto_disassembly(s.addr); break;
            case 1: goto_memory(s.addr); break;
            case 2:
                if (dbg) debugger_toggle_breakpoint(dbg, s.addr);
                request_refresh();
                break;
            case 3: Fl::copy(s.name.c_str(), (int) s.name.size(), 1); break;
            case 4:
                format_addr(text, sizeof(text), s.addr);
                Fl::copy(text, (int) strlen(text), 1);
                break;
            default: break;
        }
    }

    Fl_Scrollbar* m_bar;
    std::vector<Symbol> m_all;
    std::vector<int> m_rows;   /* indices into m_all, filtered and sorted */
    uintptr_t m_sig = 0;
    std::string m_filter;
    int m_kind = KIND_ALL;
    Sort m_sort = Sort::Address;
    bool m_ascending = true;
    size_t m_name_chars = 0;
    int m_top = 0;
    int m_hover = -1;
    std::function<void()> m_changed;
};

class SymbolsView : public Panel {
public:
    SymbolsView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "SYMBOLS")
    {
        m_filter = new FilterInput(X, Y, 10, 10);
        style_input(m_filter);
        m_filter->when(FL_WHEN_CHANGED);
        m_filter->callback(cb_filter, this);
        m_filter->tooltip("Case-insensitive substring of the symbol name\nEscape: clear");
        m_kind = new Fl_Choice(X, Y, 10, 10);
        m_kind->add("All kinds|Code|Data|Const|Other");
        m_kind->value(0);
        m_kind->textsize(12);
        m_kind->clear_visible_focus();
        m_kind->tooltip("Code: functions and labels (from code sections)\n"
                        "Data: variables (data, bss)\n"
                        "Const: read-only data\n"
                        "Other: no section (equates, absolute values)");
        m_kind->callback(cb_kind, this);
        m_open = add_button("Open Map...", "Load a map file, replacing the current symbols", cb_open);
        m_add = add_button("Add...", "Load another map file next to the current symbols\n(e.g. the OS map plus your program's)", cb_add);
        m_reload = add_button("Reload", "Reload the map files (after rebuilding the program)", cb_reload);
        m_clear = add_button("Clear", "Remove every symbol", cb_clear);
        m_bar = new Fl_Scrollbar(X, Y, 10, 10);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_list = new SymbolList(X, Y, 10, 10, m_bar);
        m_list->tooltip("Click: code opens in the disassembly, data in memory\n"
                        "Right-click: more    Click a column title to sort");
        m_list->on_change([this] { update_extra(); });
        resizable(m_list);
        end();
        resize(X, Y, W, H);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        /* Buttons on the right, filter takes what is left */
        int bx = X + W - 8;
        for (Fl_Button* b : { m_clear, m_reload, m_add, m_open }) {
            fl_font(b->labelfont(), b->labelsize());
            const int bw = (int) fl_width(b->label()) + 20;
            bx -= bw;
            b->resize(bx, by, bw, BAR_H - 8);
            bx -= 4;
        }
        const int kind_w = 100;
        bx -= 8 + kind_w;
        m_kind->resize(bx, by, kind_w, BAR_H - 8);
        m_filter->resize(X + 62, by, std::max(60, std::min(320, bx - 12 - (X + 62))), BAR_H - 8);
        const int ly = Y + HEADER_H + BAR_H;
        m_list->resize(X, ly, W - SCROLL_W, H - (ly - Y));
        m_bar->resize(X + W - SCROLL_W, ly, SCROLL_W, H - (ly - Y));
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        if (!dbg_ready()) return;
        m_list->sync();
        update_extra();
        m_list->redraw();
    }

    void draw() override
    {
        Panel::draw();
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw("FILTER", x() + 10, y() + HEADER_H + BAR_H / 2 + (int) fl_height() / 2 - fl_descent());
    }

private:
    void update_extra()
    {
        char count[32];
        if (m_list->filtered()) snprintf(count, sizeof(count), "%zu of %zu", m_list->shown(), m_list->total());
        else snprintf(count, sizeof(count), "%zu symbol%s", m_list->total(), m_list->total() == 1 ? "" : "s");
        /* Which map files they came from */
        const int maps = fz_map_count();
        char text[64];
        if (maps == 0) {
            snprintf(text, sizeof(text), "%s", count);
        } else if (maps == 1) {
            snprintf(text, sizeof(text), "%s  %s", count, base_name(fz_map_path(0)));
        } else {
            snprintf(text, sizeof(text), "%s  %d maps", count, maps);
        }
        extra(text);

        /* Full paths in the Reload tooltip */
        std::string tip = "Reload the map files (after rebuilding the program)";
        for (int i = 0; i < maps; i++) {
            tip += "\n";
            tip += fz_map_path(i);
        }
        if (tip != m_reload_tip) {
            m_reload_tip = tip;
            m_reload->tooltip(m_reload_tip.c_str());
        }
        if (maps > 0) m_reload->activate(); else m_reload->deactivate();
        if (m_list->total() > 0 || maps > 0) m_clear->activate(); else m_clear->deactivate();
    }

    static const char* base_name(const char* path)
    {
        const char* name = path;
        for (const char* p = path; *p; p++) {
            if (*p == '/' || *p == '\\') name = p + 1;
        }
        return name;
    }

    Fl_Button* add_button(const char* label, const char* tip, Fl_Callback* cb)
    {
        Fl_Button* b = new Fl_Button(x(), y(), 10, 10, label);
        style_button(b);
        b->tooltip(tip);
        b->callback(cb, this);
        return b;
    }

    void after_change()
    {
        refresh(fz_paused(), true);
    }

    static void cb_open(Fl_Widget*, void* data)
    {
        if (map_open_dialog(false)) static_cast<SymbolsView*>(data)->after_change();
    }

    static void cb_add(Fl_Widget*, void* data)
    {
        if (map_open_dialog(true)) static_cast<SymbolsView*>(data)->after_change();
    }

    static void cb_reload(Fl_Widget*, void* data)
    {
        map_reload();
        static_cast<SymbolsView*>(data)->after_change();
    }

    static void cb_clear(Fl_Widget*, void* data)
    {
        map_clear();
        static_cast<SymbolsView*>(data)->after_change();
    }

    static void cb_filter(Fl_Widget*, void* data)
    {
        SymbolsView* self = static_cast<SymbolsView*>(data);
        self->m_list->filter(self->m_filter->value());
        self->update_extra();
    }

    static void cb_kind(Fl_Widget*, void* data)
    {
        SymbolsView* self = static_cast<SymbolsView*>(data);
        /* Menu order: All, Code, Data, Const, Other */
        static const int kinds[] = { KIND_ALL, SYM_KIND_CODE, SYM_KIND_DATA, SYM_KIND_CONST, SYM_KIND_UNKNOWN };
        const int v = self->m_kind->value();
        self->m_list->filter_kind(v >= 0 && v < 5 ? kinds[v] : KIND_ALL);
        self->update_extra();
    }

    static void cb_bar(Fl_Widget*, void* data)
    {
        SymbolsView* self = static_cast<SymbolsView*>(data);
        self->m_list->scroll_to(self->m_bar->value());
    }

    FilterInput* m_filter;
    Fl_Choice* m_kind;
    Fl_Button* m_open;
    Fl_Button* m_add;
    Fl_Button* m_reload;
    Fl_Button* m_clear;
    std::string m_reload_tip;
    Fl_Scrollbar* m_bar;
    SymbolList* m_list;
};

Panel* make_symbols(int x, int y, int w, int h)
{
    return new SymbolsView(x, y, w, h);
}

ToolViewRegistrar s_reg(TOOL_CODE, 10, "Symbols", make_symbols);

} // namespace

/* Every view reads the symbol table live; just redraw everything */
static void symbols_changed()
{
    request_refresh();
    tool_windows_refresh(fz_paused(), true);
}

bool map_open_dialog(bool append)
{
    static std::string s_dir;
    Fl_Native_File_Chooser chooser;
    chooser.title(append ? "Add Map File" : "Open Map File");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("Map files\t*.map\nAll files\t*");
    /* Start in the folder picked last time, else the HostFS folder, where
     * the programs being tested (and their maps) usually live */
    std::string start = s_dir;
    if (start.empty() && config.arguments.hostfs_path) {
        char abs[FL_PATH_MAX];
        fl_filename_absolute(abs, sizeof(abs), config.arguments.hostfs_path);
        start = abs;
    }
    if (!start.empty()) chooser.directory(start.c_str());
    if (chooser.show() != 0 || chooser.filename() == nullptr) return false;
    const std::string path = chooser.filename();
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) s_dir = path.substr(0, slash);

    const int before = fz_symbol_count();
    if (!fz_map_open(path.c_str(), append)) {
        notif_show("Could not load %s", path.c_str());
        return false;
    }
    const int loaded = append ? fz_symbol_count() - before : fz_symbol_count();
    const char* name = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
    notif_show("%d symbols loaded from %s", loaded, name);
    symbols_changed();
    return true;
}

void map_reload()
{
    const int files = fz_map_count();
    if (files == 0) return;
    const int ok = fz_map_reload();
    if (ok == files) notif_show("Reloaded %d symbols", fz_symbol_count());
    else notif_show("Reloaded %d of %d map files", ok, files);
    symbols_changed();
}

void map_clear()
{
    fz_map_clear();
    notif_show("Symbols cleared");
    symbols_changed();
}

#endif

} // namespace zui
