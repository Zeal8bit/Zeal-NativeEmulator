/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* UART console (Hardware window): everything the machine sends on the UART,
 * captured by bridge_input.c, shown as a scrollable terminal. */

#include <cstdio>
#include <cstring>
#include <string>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "tool_window.h"

extern "C" {
#include "frontend/sdl-fltk/bridge/console.h"
#include "frontend/sdl-fltk/bridge/input.h"
}

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

#include "cp437.inc"

/** Append one console byte as UTF-8 (CP437 for control and high bytes). */
static void append_char(std::string& out, uint8_t b)
{
    if (b >= 0x20 && b < 0x7F) {
        out += (char) b;
        return;
    }
    char utf8[8];
    const unsigned cp = s_cp437[b] ? s_cp437[b] : ' ';
    const int n = fl_utf8encode(cp, utf8);
    out.append(utf8, (size_t) n);
}

static void format_time(char* out, size_t size, uint64_t ns)
{
    snprintf(out, size, "%10.6f", (double) ns / 1e9);
}

static std::string line_text(const fz_console_line_t& line, bool time)
{
    std::string s;
    if (time) {
        char t[32];
        format_time(t, sizeof(t), line.time_ns);
        s += t;
        s += "  ";
    }
    for (int i = 0; i < line.len; i++) append_char(s, (uint8_t) line.text[i]);
    return s;
}

static std::string all_text(bool time)
{
    std::string s;
    const uint64_t first = fz_console_first();
    const int count = fz_console_count();
    for (int i = 0; i < count; i++) {
        fz_console_line_t line;
        if (!fz_console_line(first + (uint64_t) i, &line)) continue;
        s += line_text(line, time);
        s += '\n';
    }
    return s;
}

/* ------------------------------------------------------------------ */

class ConsoleText : public Fl_Widget {
public:
    ConsoleText(int X, int Y, int W, int H, Fl_Scrollbar* bar) : Fl_Widget(X, Y, W, H), m_bar(bar) {}

    bool follow = true;
    bool time = false;
    std::function<void(bool)> on_follow;
    std::function<void()> on_menu;

    int rows() const
    {
        const int lh = data_line_height();
        return lh > 0 ? (h() - 8) / lh : 1;
    }

    /** Clamp, apply follow and update the scrollbar. */
    void sync()
    {
        const uint64_t first = fz_console_first();
        const int count = fz_console_count();
        const int max_top = count > rows() ? count - rows() : 0;
        if (follow || m_top < first) m_top = first + (uint64_t) (follow ? max_top : 0);
        if (m_top > first + (uint64_t) max_top) m_top = first + (uint64_t) max_top;
        m_bar->value((int) (m_top - first), rows(), 0, count > rows() ? count : rows());
    }

    void scroll_to(int rel)
    {
        const int count = fz_console_count();
        const int max_top = count > rows() ? count - rows() : 0;
        if (rel < 0) rel = 0;
        if (rel > max_top) rel = max_top;
        m_top = fz_console_first() + (uint64_t) rel;
        set_follow(rel >= max_top);
        sync();
        redraw();
    }

    int selected() const { return m_sel_valid ? 1 : 0; }

    bool selected_line(fz_console_line_t* out) const
    {
        return m_sel_valid && fz_console_line(m_sel, out);
    }

    void cleared()
    {
        m_top = 0;
        m_sel_valid = false;
        set_follow(true);
    }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_font(DATA_FONT, DATA_SIZE);
        const int lh = data_line_height();
        const int asc = text_baseline(lh);
        const int cw = char_width();
        const int tx = x() + 8 + (time ? cw * 12 : 0);
        const int count = fz_console_count();
        const uint64_t first = fz_console_first();
        if (count == 0) {
            fl_color(COLOR_DIM);
            fl_draw("No UART output yet. Bytes the machine sends on the UART appear here.",
                    x() + 8, y() + 4 + asc);
        }
        std::string text;
        char ts[32];
        for (int r = 0; r <= rows(); r++) {
            const uint64_t n = m_top + (uint64_t) r;
            if (n >= first + (uint64_t) count) break;
            fz_console_line_t line;
            if (!fz_console_line(n, &line)) continue;
            const int ry = y() + 4 + r * lh;
            if (m_sel_valid && n == m_sel) {
                fl_color(COLOR_SELECT);
                fl_rectf(x(), ry, w(), lh);
            } else if (n == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
            }
            if (time) {
                format_time(ts, sizeof(ts), line.time_ns);
                fl_color(COLOR_DIM);
                fl_draw(ts, x() + 8, ry + asc);
            }
            text.clear();
            for (int i = 0; i < line.len; i++) append_char(text, (uint8_t) line.text[i]);
            fl_color(COLOR_TEXT);
            fl_draw(text.c_str(), (int) text.size(), tx, ry + asc);
        }
        if (time) {
            fl_color(COLOR_BORDER);
            fl_yxline(tx - cw, y(), y() + h());
        }
        fl_pop_clip();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_hover = UINT64_MAX;
                redraw();
                return 1;
            case FL_MOVE: {
                const uint64_t n = line_at(Fl::event_y());
                if (n != m_hover) {
                    m_hover = n;
                    redraw();
                }
                return 1;
            }
            case FL_MOUSEWHEEL: {
                const int rel = (int) (m_top - fz_console_first());
                scroll_to(rel + Fl::event_dy() * 3);
                return 1;
            }
            case FL_PUSH: {
                const uint64_t n = line_at(Fl::event_y());
                if (n != UINT64_MAX) {
                    m_sel = n;
                    m_sel_valid = true;
                } else if (Fl::event_button() != FL_RIGHT_MOUSE) {
                    m_sel_valid = false;
                }
                redraw();
                if (Fl::event_button() == FL_RIGHT_MOUSE && on_menu) on_menu();
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    uint64_t line_at(int ey) const
    {
        const int lh = data_line_height();
        if (lh <= 0 || ey < y() + 4) return UINT64_MAX;
        const uint64_t n = m_top + (uint64_t) ((ey - y() - 4) / lh);
        return n < fz_console_first() + (uint64_t) fz_console_count() ? n : UINT64_MAX;
    }

    void set_follow(bool f)
    {
        if (f == follow) return;
        follow = f;
        if (on_follow) on_follow(f);
    }

    Fl_Scrollbar* m_bar;
    uint64_t m_top = 0;
    uint64_t m_sel = 0;
    bool m_sel_valid = false;
    uint64_t m_hover = UINT64_MAX;
};

/* ------------------------------------------------------------------ */

class ConsoleView : public Panel {
public:
    static constexpr const char* KEY_FOLLOW = "FLTK_CONSOLE_FOLLOW";
    static constexpr const char* KEY_TIME = "FLTK_CONSOLE_TIME";

    ConsoleView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "UART CONSOLE")
    {
        m_follow = new Fl_Check_Button(X, Y, 10, 10, "Follow");
        style_check(m_follow);
        m_follow->tooltip("Keep the newest output in view");
        m_follow->callback(cb_follow, this);
        m_time = new Fl_Check_Button(X, Y, 10, 10, "Time");
        style_check(m_time);
        m_time->tooltip("Show the emulated time (seconds) at which each line started");
        m_time->callback(cb_time, this);
        m_clear = new Fl_Button(X, Y, 10, 10, "Clear");
        style_button(m_clear);
        m_clear->callback(cb_clear, this);
        m_copy = new Fl_Button(X, Y, 10, 10, "Copy All");
        style_button(m_copy);
        m_copy->callback(cb_copy, this);
        m_save = new Fl_Button(X, Y, 10, 10, "Save...");
        style_button(m_save);
        m_save->callback(cb_save, this);
        m_bar = new Fl_Scrollbar(X, Y, 10, 10);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_text = new ConsoleText(X, Y, 10, 10, m_bar);
        m_text->tooltip("Click: select a line    Right-click: copy, clear, save");
        m_text->follow = config_get(KEY_FOLLOW, 1) != 0;
        m_text->time = config_get(KEY_TIME, 0) != 0;
        m_text->on_follow = [this](bool f) { m_follow->value(f ? 1 : 0); };
        m_text->on_menu = [this] { context_menu(); };
        m_follow->value(m_text->follow ? 1 : 0);
        m_time->value(m_text->time ? 1 : 0);
        resizable(m_text);
        end();
        resize(X, Y, W, H);

        persist_register([this](rini_config* ini) {
            rini_set_config_value(ini, KEY_FOLLOW, m_text->follow ? 1 : 0, "Console view: follow output");
            rini_set_config_value(ini, KEY_TIME, m_text->time ? 1 : 0, "Console view: timestamps");
        });
        Fl::add_timeout(0.1, cb_tick, this);
    }

    ~ConsoleView() override
    {
        Fl::remove_timeout(cb_tick, this);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int by = Y + HEADER_H + 4;
        const int bh = BAR_H - 8;
        m_follow->resize(X + 8, by, 70, bh);
        m_time->resize(X + 80, by, 60, bh);
        m_clear->resize(X + 150, by, 56, bh);
        m_copy->resize(X + 210, by, 72, bh);
        m_save->resize(X + 286, by, 64, bh);
        const int ty = Y + HEADER_H + BAR_H;
        m_bar->resize(X + W - 14, ty, 14, H - (ty - Y));
        m_text->resize(X, ty, W - 14, H - (ty - Y));
        m_text->sync();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        if (force || fz_console_serial() != m_serial) update();
    }

private:
    void update()
    {
        m_serial = fz_console_serial();
        char text[64];
        snprintf(text, sizeof(text), "%d lines  %llu bytes", fz_console_count(),
                 (unsigned long long) fz_console_bytes());
        extra(text);
        m_text->sync();
        redraw();
    }

    void copy_all()
    {
        const std::string s = all_text(m_text->time);
        Fl::copy(s.c_str(), (int) s.size(), 1);
        notif_show("Copied %d console lines", fz_console_count());
    }

    void clear()
    {
        fz_console_clear();
        m_text->cleared();
        update();
    }

    void save()
    {
        Fl_Native_File_Chooser chooser;
        chooser.title("Save UART Console");
        chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        chooser.filter("Text\t*.{txt,log}");
        chooser.preset_file("uart.log");
        if (chooser.show() != 0 || chooser.filename() == nullptr) return;
        FILE* f = fl_fopen(chooser.filename(), "wb");
        if (f == nullptr) {
            notif_show("Could not write %s", chooser.filename());
            return;
        }
        const std::string s = all_text(m_text->time);
        const bool ok = fwrite(s.data(), 1, s.size(), f) == s.size();
        fclose(f);
        notif_show(ok ? "Console saved to %s" : "Could not write %s", chooser.filename());
    }

    void context_menu()
    {
        fz_console_line_t line;
        const bool have_line = m_text->selected_line(&line);
        Fl_Menu_Item items[] = {
            { "Copy Line", 0, nullptr, nullptr, have_line ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
            { "Copy All", 0, nullptr, nullptr, FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Follow Output", 0, nullptr, nullptr, FL_MENU_TOGGLE | (m_text->follow ? FL_MENU_VALUE : 0), 0, 0, 0, 0 },
            { "Show Timestamps", 0, nullptr, nullptr, FL_MENU_TOGGLE | (m_text->time ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Clear", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "Save...", 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        switch ((int) (picked - items)) {
            case 0: {
                const std::string s = line_text(line, m_text->time);
                Fl::copy(s.c_str(), (int) s.size(), 1);
                break;
            }
            case 1: copy_all(); break;
            case 2: set_follow(!m_text->follow); break;
            case 3: set_time(!m_text->time); break;
            case 4: clear(); break;
            case 5: save(); break;
            default: break;
        }
    }

    void set_follow(bool f)
    {
        m_text->follow = f;
        m_follow->value(f ? 1 : 0);
        update();
    }

    void set_time(bool t)
    {
        m_text->time = t;
        m_time->value(t ? 1 : 0);
        m_text->redraw();
    }

    static void cb_tick(void* data)
    {
        ConsoleView* self = (ConsoleView*) data;
        /* Faster than the tool window refresh so output appears promptly */
        if (self->visible_r() && fz_console_serial() != self->m_serial) self->update();
        Fl::repeat_timeout(0.1, cb_tick, data);
    }

    static void cb_follow(Fl_Widget*, void* data)
    {
        ConsoleView* self = (ConsoleView*) data;
        self->set_follow(self->m_follow->value() != 0);
    }

    static void cb_time(Fl_Widget*, void* data)
    {
        ConsoleView* self = (ConsoleView*) data;
        self->set_time(self->m_time->value() != 0);
    }

    static void cb_clear(Fl_Widget*, void* data) { ((ConsoleView*) data)->clear(); }
    static void cb_copy(Fl_Widget*, void* data) { ((ConsoleView*) data)->copy_all(); }
    static void cb_save(Fl_Widget*, void* data) { ((ConsoleView*) data)->save(); }

    static void cb_bar(Fl_Widget*, void* data)
    {
        ConsoleView* self = (ConsoleView*) data;
        self->m_text->scroll_to(self->m_bar->value());
    }

    Fl_Check_Button* m_follow;
    Fl_Check_Button* m_time;
    Fl_Button* m_clear;
    Fl_Button* m_copy;
    Fl_Button* m_save;
    Fl_Scrollbar* m_bar;
    ConsoleText* m_text;
    uint64_t m_serial = UINT64_MAX;
};

static Panel* make_console(int x, int y, int w, int h)
{
    return new ConsoleView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_HARDWARE, 50, "Console", make_console);

#endif

} // namespace zui
