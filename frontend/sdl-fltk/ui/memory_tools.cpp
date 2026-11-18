/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Memory editor tools: pattern search, the "changed since last pause"
 * candidate search and the fill / save / load / go-to dialogs. */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "memory_tools.h"

namespace zui {
namespace memtools {

/* ------------------------------------------------------------------ */
/*  Pattern search                                                     */
/* ------------------------------------------------------------------ */

bool Pattern::matches(const uint8_t* at) const
{
    for (size_t i = 0; i < bytes.size(); i++) {
        if (mask[i] && at[i] != bytes[i]) return false;
    }
    return true;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool fail(std::string* error, const char* msg)
{
    if (error) *error = msg;
    return false;
}

bool parse_pattern(const char* text, Pattern& out, std::string* error, bool allow_wildcards)
{
    out.bytes.clear();
    out.mask.clear();
    if (text == nullptr) text = "";
    const char* p = text;
    while (*p) {
        if (isspace((unsigned char) *p) || *p == ',') {
            p++;
            continue;
        }
        if (*p == '"') {
            p++;
            bool closed = false;
            while (*p) {
                if (*p == '"') {
                    closed = true;
                    p++;
                    break;
                }
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
                out.bytes.push_back((uint8_t) *p);
                out.mask.push_back(1);
                p++;
            }
            if (!closed) return fail(error, "Missing closing quote");
            continue;
        }
        if (*p == '?') {
            if (!allow_wildcards) return fail(error, "Wildcards are not allowed here");
            p++;
            if (*p == '?') p++;
            out.bytes.push_back(0);
            out.mask.push_back(0);
            continue;
        }
        /* Hex token: optional $ or 0x prefix, then a run of hex digits */
        if (*p == '$') p++;
        else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && hex_digit(p[2]) >= 0) p += 2;
        const char* start = p;
        while (hex_digit(*p) >= 0) p++;
        const size_t n = (size_t) (p - start);
        if (n == 0 || (*p && !isspace((unsigned char) *p) && *p != ',' && *p != '"' && *p != '?')) {
            return fail(error, "Use hex bytes (3E 22 ?\?) or a \"string\"");
        }
        if (n == 1) {
            out.bytes.push_back((uint8_t) hex_digit(start[0]));
            out.mask.push_back(1);
            continue;
        }
        if (n % 2) return fail(error, "Odd number of hex digits");
        for (size_t i = 0; i < n; i += 2) {
            out.bytes.push_back((uint8_t) (hex_digit(start[i]) << 4 | hex_digit(start[i + 1])));
            out.mask.push_back(1);
        }
    }
    if (out.bytes.empty()) return fail(error, "Empty pattern");
    return true;
}

int find(const uint8_t* mem, int size, const Pattern& p, int from, bool forward, bool* wrapped)
{
    if (wrapped) *wrapped = false;
    const int n = p.size();
    if (n == 0 || n > size) return -1;
    const int count = size - n + 1; /* possible start positions */
    int start;
    if (forward) start = from < 0 ? 0 : from >= count ? 0 : from;
    else start = from >= count ? count - 1 : from < 0 ? count - 1 : from;
    for (int k = 0; k < count; k++) {
        const int pos = forward ? (start + k) % count : (start - k + count) % count;
        if (p.matches(mem + pos)) {
            if (wrapped) *wrapped = forward ? pos < from : pos > from;
            return pos;
        }
    }
    return -1;
}

bool parse_byte(const char* text, uint8_t* out)
{
    if (text == nullptr) return false;
    while (isspace((unsigned char) *text)) text++;
    char* end = nullptr;
    unsigned long v;
    if (*text == '#') {
        if (!isdigit((unsigned char) text[1])) return false;
        v = strtoul(text + 1, &end, 10);
    } else {
        if (*text == '$') text++;
        else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
        if (hex_digit(*text) < 0) return false;
        v = strtoul(text, &end, 16);
    }
    while (end && isspace((unsigned char) *end)) end++;
    if (end == nullptr || *end != '\0' || v > 0xFF) return false;
    *out = (uint8_t) v;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Changed-bytes search                                               */
/* ------------------------------------------------------------------ */

void ChangeSearch::start(int size)
{
    m_candidates.resize((size_t) size);
    for (int i = 0; i < size; i++) m_candidates[(size_t) i] = (uint16_t) i;
    m_active = true;
    m_rounds = 0;
}

bool ChangeSearch::passes(Filter filter, uint8_t value, uint8_t prev, uint8_t cur)
{
    switch (filter) {
        case Filter::Changed:   return cur != prev;
        case Filter::Unchanged: return cur == prev;
        case Filter::Increased: return cur > prev;
        case Filter::Decreased: return cur < prev;
        case Filter::Equal:     return cur == value;
    }
    return false;
}

void ChangeSearch::narrow(Filter filter, uint8_t value, const uint8_t* prev, const uint8_t* cur)
{
    if (!m_active) start();
    size_t keep = 0;
    for (size_t i = 0; i < m_candidates.size(); i++) {
        const uint16_t a = m_candidates[i];
        if (passes(filter, value, prev ? prev[a] : cur[a], cur[a])) m_candidates[keep++] = a;
    }
    m_candidates.resize(keep);
    m_rounds++;
}

} // namespace memtools
} // namespace zui

/* ================================================================== */
/*  User interface (needs the debugger)                                */
/* ================================================================== */

#if CONFIG_ENABLE_DEBUGGER

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_draw.H>
#include "panel.h"

namespace zui {
namespace memtools {

static std::string hex16s(unsigned v)
{
    char t[8];
    hex16(t, sizeof(t), v);
    return t;
}

static void place_over_main(Fl_Window* win)
{
    MainWindow* mw = main_window();
    if (mw && mw->shown()) {
        win->position(mw->x() + (mw->w() - win->w()) / 2, mw->y() + (mw->h() - win->h()) / 3);
    } else {
        win->hotspot(win);
    }
}

static void select_all(Fl_Input* in)
{
#if FL_API_VERSION >= 10400
    in->insert_position(in->size(), 0);
#else
    in->position(in->size(), 0);
#endif
}

/* ------------------------------------------------------------------ */
/*  Themed modal form                                                  */
/* ------------------------------------------------------------------ */

class FormDialog : public Fl_Double_Window {
public:
    static constexpr int W = 400;
    static constexpr int ROW_H = 30;
    static constexpr int LABEL_W = 92;

    FormDialog(const char* title, const char* ok_label, const std::vector<const char*>& labels,
               const char* note)
        : Fl_Double_Window(W, 100, title), m_title(title)
    {
        color(COLOR_SURFACE);
        int y = HEADER_H + 12;
        for (const char* label : labels) {
            Fl_Box* l = new Fl_Box(12, y, LABEL_W, ROW_H - 6, label);
            l->labelsize(12);
            l->labelcolor(COLOR_AMBER);
            l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            Fl_Input* in = new Fl_Input(12 + LABEL_W, y, W - 24 - LABEL_W, ROW_H - 6);
            style_input(in);
            m_inputs.push_back(in);
            y += ROW_H;
        }
        m_note = new Fl_Box(12, y, W - 24, 34);
        m_note->labelsize(12);
        m_note->labelcolor(COLOR_DIM);
        m_note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_TOP);
        m_note->copy_label(note ? note : "");
        y += 34;
        m_message = new Fl_Box(12, y, W - 24, 34);
        m_message->labelsize(12);
        m_message->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_TOP);
        y += 38;
        Fl_Button* cancel = new Fl_Button(W - 12 - 2 * 92 - 8, y, 92, 26, "Cancel");
        style_button(cancel);
        cancel->callback(cb_cancel, this);
        Fl_Button* ok = new Fl_Button(W - 12 - 92, y, 92, 26, ok_label);
        style_button(ok);
        ok->color(COLOR_SELECT);
        ok->labelcolor(COLOR_AMBER);
        ok->shortcut(FL_Enter);
        ok->callback(cb_ok, this);
        end();
        size(W, y + 26 + 12);
        set_modal();
        callback(cb_cancel, this);
    }

    Fl_Input* input(int i) const { return m_inputs[(size_t) i]; }
    const char* value(int i) const { return m_inputs[(size_t) i]->value(); }
    void value(int i, const char* text) { m_inputs[(size_t) i]->value(text); }
    void note(const char* text) { m_note->copy_label(text); redraw(); }
    void error(const char* text) { message(text, COLOR_BREAK); }
    void warn(const char* text) { message(text, COLOR_AMBER); }

    /** Show modally until OK (accepted by `accept`) or Cancel/Escape. */
    bool run(const std::function<bool(FormDialog&)>& accept)
    {
        place_over_main(this);
        show();
        if (!m_inputs.empty()) {
            m_inputs[0]->take_focus();
            select_all(m_inputs[0]);
        }
        m_accept = &accept;
        m_result = false;
        while (shown()) Fl::wait();
        m_accept = nullptr;
        return m_result;
    }

    void draw() override
    {
        Fl_Double_Window::draw();
        draw_header(0, 0, w(), m_title);
    }

private:
    void message(const char* text, Fl_Color c)
    {
        m_message->labelcolor(c);
        m_message->copy_label(text);
        redraw();
    }

    static void cb_ok(Fl_Widget*, void* data)
    {
        FormDialog* self = static_cast<FormDialog*>(data);
        if (self->m_accept && !(*self->m_accept)(*self)) return;
        self->m_result = true;
        self->hide();
    }

    static void cb_cancel(Fl_Widget*, void* data)
    {
        FormDialog* self = static_cast<FormDialog*>(data);
        self->m_result = false;
        self->hide();
    }

    const char* m_title;
    std::vector<Fl_Input*> m_inputs;
    Fl_Box* m_note;
    Fl_Box* m_message;
    const std::function<bool(FormDialog&)>* m_accept = nullptr;
    bool m_result = false;
};

/** Parse a start/end pair from two fields; reports errors in the dialog. */
static bool parse_range(FormDialog& d, int first, unsigned* start, unsigned* end)
{
    hwaddr s, e;
    if (!parse_address(d.value(first), &s)) {
        d.error("Start: hex address ($C000) or symbol");
        return false;
    }
    if (!parse_address(d.value(first + 1), &e)) {
        d.error("End: hex address ($C0FF) or symbol");
        return false;
    }
    if (s > 0xFFFF || e > 0xFFFF || e < s) {
        d.error("End must be at or after Start, both within $0000-$FFFF");
        return false;
    }
    *start = (unsigned) s;
    *end = (unsigned) e;
    return true;
}

static std::string range_text(unsigned start, unsigned end)
{
    char t[64];
    snprintf(t, sizeof(t), "$%s-$%s (%u bytes)", hex16s(start).c_str(), hex16s(end).c_str(), end - start + 1);
    return t;
}

void fill_dialog(unsigned start, unsigned end, const Writer& write)
{
    FormDialog d("FILL MEMORY", "Fill", { "Start $", "End $", "Value" },
                 "Value: a hex byte (00), a byte pattern (DE AD BE EF) or a \"string\", repeated over the range.");
    d.value(0, hex16s(start).c_str());
    d.value(1, hex16s(end).c_str());
    d.value(2, "00");
    unsigned s = start, e = end;
    Pattern pattern;
    const bool ok = d.run([&](FormDialog& f) {
        if (!parse_range(f, 0, &s, &e)) return false;
        std::string err;
        if (!parse_pattern(f.value(2), pattern, &err, false)) {
            f.error(("Value: " + err).c_str());
            return false;
        }
        return true;
    });
    if (!ok) return;
    std::vector<uint8_t> data(e - s + 1);
    for (size_t i = 0; i < data.size(); i++) data[i] = pattern.bytes[i % pattern.bytes.size()];
    write(s, data);
    notif_show("Filled %s", range_text(s, e).c_str());
}

void save_range_dialog(unsigned start, unsigned end)
{
    FormDialog d("SAVE RANGE", "Save...", { "Start $", "End $" },
                 "Saves the bytes as the CPU sees them (current MMU mapping) to a binary file.");
    d.value(0, hex16s(start).c_str());
    d.value(1, hex16s(end).c_str());
    unsigned s = start, e = end;
    if (!d.run([&](FormDialog& f) { return parse_range(f, 0, &s, &e); })) return;

    Fl_Native_File_Chooser chooser;
    chooser.title("Save Memory Range");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::NEW_FOLDER);
    char name[64];
    snprintf(name, sizeof(name), "mem_%s_%s.bin", hex16s(s).c_str(), hex16s(e).c_str());
    chooser.preset_file(name);
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;

    std::vector<uint8_t> data(e - s + 1);
    debugger_read_memory(fz_debugger(), (hwaddr) s, (int) data.size(), data.data());
    FILE* f = fopen(chooser.filename(), "wb");
    const bool written = f && fwrite(data.data(), 1, data.size(), f) == data.size();
    if (f && fclose(f) != 0) {
        notif_show("Could not write %s", chooser.filename());
        return;
    }
    if (written) notif_show("Saved %s", range_text(s, e).c_str());
    else notif_show("Could not write %s", chooser.filename());
}

void load_file_dialog(unsigned addr, const Writer& write)
{
    Fl_Native_File_Chooser chooser;
    chooser.title("Load File into Memory");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (chooser.show() != 0 || chooser.filename() == nullptr) return;
    const std::string path = chooser.filename();

    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        notif_show("Could not open %s", path.c_str());
        return;
    }
    std::vector<uint8_t> data;
    uint8_t chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0 && data.size() <= 0x10000) {
        data.insert(data.end(), chunk, chunk + n);
    }
    fclose(f);
    if (data.empty()) {
        notif_show("%s is empty", path.c_str());
        return;
    }
    const bool huge = data.size() > 0x10000;
    if (huge) data.resize(0x10000);

    const size_t slash = path.find_last_of("/\\");
    const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    char note[256];
    snprintf(note, sizeof(note), "%s: %s%zu bytes", base.c_str(), huge ? "first " : "", data.size());
    FormDialog d("LOAD FILE", "Load", { "Address $" }, note);
    d.value(0, hex16s(addr).c_str());
    unsigned at = addr;
    std::string confirmed; /* address text for which truncation was accepted */
    const bool ok = d.run([&](FormDialog& form) {
        hwaddr a;
        if (!parse_address(form.value(0), &a) || a > 0xFFFF) {
            form.error("Address: hex ($C000) or symbol");
            return false;
        }
        at = (unsigned) a;
        const size_t fits = 0x10000 - at;
        if (data.size() > fits && confirmed != form.value(0)) {
            char t[160];
            snprintf(t, sizeof(t), "Crosses $FFFF: only %zu of %zu bytes fit ($%s-$FFFF). Press Load again to load them.",
                     fits, data.size(), hex16s(at).c_str());
            form.warn(t);
            confirmed = form.value(0);
            return false;
        }
        return true;
    });
    if (!ok) return;
    const size_t fits = 0x10000 - at;
    if (data.size() > fits) data.resize(fits);
    write(at, data);
    notif_show("Loaded %s at %s", base.c_str(), range_text(at, at + (unsigned) data.size() - 1).c_str());
}

bool address_dialog(const char* title, unsigned initial, unsigned* out)
{
    FormDialog d(title, "Go", { "Address $" }, "Hex address ($C000) or a debugger symbol.");
    d.value(0, hex16s(initial).c_str());
    hwaddr a = 0;
    const bool ok = d.run([&](FormDialog& f) {
        if (parse_address(f.value(0), &a) && a <= 0xFFFF) return true;
        f.error("Not a hex address or known symbol");
        return false;
    });
    if (ok) *out = (unsigned) a;
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Changed-bytes search window                                        */
/* ------------------------------------------------------------------ */

class CandidateList : public Fl_Widget {
public:
    CandidateList(int X, int Y, int W, int H, Fl_Scrollbar* bar) : Fl_Widget(X, Y, W, H), m_bar(bar) {}

    const std::vector<uint16_t>* items = nullptr;
    const SnapshotSource* source = nullptr;

    int rows() const { return (h() - 4) / data_line_height(); }
    int count() const { return items ? (int) items->size() : 0; }

    void scroll_to(int top)
    {
        m_top = top;
        clamp();
        redraw();
    }

    void sync()
    {
        clamp();
        m_bar->value(m_top, rows(), 0, count() > rows() ? count() : rows());
        redraw();
    }

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        const uint8_t* prev = nullptr;
        const uint8_t* cur = nullptr;
        const bool have = source && source->get && source->get(&prev, &cur);
        const int lh = data_line_height();
        const int asc = text_baseline(lh);
        const int cw = char_width();
        fl_font(DATA_FONT, DATA_SIZE);
        char t[16];
        for (int r = 0; r < rows() && m_top + r < count(); r++) {
            const int i = m_top + r;
            const unsigned a = (*items)[(size_t) i];
            const int yy = y() + 2 + r * lh;
            if (i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), yy, w(), lh);
            }
            int xx = x() + 6;
            hex16(t, sizeof(t), a);
            fl_color(COLOR_DIM);
            fl_draw("$", xx, yy + asc);
            fl_color(COLOR_CYAN);
            fl_draw(t, xx + cw, yy + asc);
            xx += cw * 7;
            if (have) {
                const uint8_t p = prev ? prev[a] : cur[a];
                const uint8_t c = cur[a];
                hex8(t, sizeof(t), p);
                fl_color(COLOR_DIM);
                fl_draw("$", xx, yy + asc);
                fl_color(COLOR_TEXT);
                fl_draw(t, xx + cw, yy + asc);
                fl_color(COLOR_DIM);
                fl_draw("\xE2\x86\x92", xx + cw * 4, yy + asc); /* arrow */
                hex8(t, sizeof(t), c);
                fl_draw("$", xx + cw * 6, yy + asc);
                fl_color(c != p ? COLOR_CHANGED : COLOR_TEXT);
                fl_draw(t, xx + cw * 7, yy + asc);
                snprintf(t, sizeof(t), "#%u", c);
                fl_color(COLOR_DIM);
                fl_draw(t, xx + cw * 10, yy + asc);
                xx += cw * 15;
            }
            const char* sym = debugger_get_symbol(fz_debugger(), (hwaddr) a);
            if (sym) {
                fl_color(COLOR_AMBER);
                fl_draw(sym, xx, yy + asc);
            }
        }
        if (count() == 0) {
            fl_font(FL_HELVETICA, 12);
            fl_color(COLOR_DIM);
            fl_draw(items ? "No candidates left. Reset to start over." : "Pick a filter and press Apply.",
                    x() + 8, y() + 4, w() - 16, 40, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        }
        fl_pop_clip();
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_hover = -1;
                redraw();
                return 1;
            case FL_MOVE: {
                const int i = index_at(Fl::event_y());
                if (i != m_hover) {
                    m_hover = i;
                    redraw();
                }
                return 1;
            }
            case FL_PUSH: {
                const int i = index_at(Fl::event_y());
                if (i >= 0 && source && source->go) source->go((*items)[(size_t) i]);
                return 1;
            }
            case FL_MOUSEWHEEL:
                m_top += Fl::event_dy() * 3;
                sync();
                return 1;
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        sync();
    }

private:
    int index_at(int ey) const
    {
        const int r = (ey - y() - 2) / data_line_height();
        if (ey < y() + 2 || r >= rows()) return -1;
        const int i = m_top + r;
        return i < count() ? i : -1;
    }

    void clamp()
    {
        if (m_top > count() - rows()) m_top = count() - rows();
        if (m_top < 0) m_top = 0;
    }

    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_hover = -1;
};

class ChangeSearchWindow : public Fl_Double_Window {
public:
    static constexpr int W = 380;
    static constexpr int H = 460;

    ChangeSearchWindow() : Fl_Double_Window(W, H, "Changed Bytes")
    {
        color(COLOR_SURFACE);
        int y = HEADER_H + 6;
        m_filter = new Fl_Choice(8, y, 132, BAR_H - 8);
        m_filter->add("Changed|Unchanged|Increased|Decreased|Equal to");
        m_filter->value(0);
        m_filter->textsize(12);
        m_filter->clear_visible_focus();
        m_filter->tooltip("Compare each candidate's byte at this pause with the previous pause");
        m_filter->callback(cb_filter, this);
        m_value = new Fl_Input(146, y, 60, BAR_H - 8);
        style_input(m_value);
        m_value->value("00");
        m_value->tooltip("Byte value: hex (3E, $3E) or decimal with # (#62)");
        m_apply = new Fl_Button(212, y, 76, BAR_H - 8, "Apply");
        style_button(m_apply);
        m_apply->labelcolor(COLOR_AMBER);
        m_apply->tooltip("Keep only the candidates that pass the filter");
        m_apply->callback(cb_apply, this);
        m_reset = new Fl_Button(294, y, 78, BAR_H - 8, "Reset");
        style_button(m_reset);
        m_reset->tooltip("Forget the candidates and start over with all 65536 bytes");
        m_reset->callback(cb_reset, this);
        y += BAR_H - 4;
        m_auto = new Fl_Check_Button(8, y, W - 16, BAR_H - 8, "Narrow with this filter at every pause");
        style_check(m_auto);
        y += BAR_H - 6;
        m_status = new Fl_Box(8, y, W - 16, 20);
        m_status->labelsize(12);
        m_status->labelcolor(COLOR_DIM);
        m_status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        y += 24;
        m_bar = new Fl_Scrollbar(W - 14, y, 14, H - y);
        m_bar->type(FL_VERTICAL);
        m_bar->linesize(1);
        m_bar->color(COLOR_SURFACE);
        m_bar->callback(cb_bar, this);
        m_list = new CandidateList(0, y, W - 14, H - y, m_bar);
        m_list->source = &m_source;
        resizable(m_list);
        end();
        size_range(320, 240);
        set_non_modal();
        update_filter();
        update();
    }

    void source(const SnapshotSource& src) { m_source = src; }

    void paused()
    {
        if (m_search.active() && m_auto->value()) apply();
        else update();
    }

    void draw() override
    {
        Fl_Double_Window::draw();
        char extra[64] = "";
        if (m_search.active()) {
            snprintf(extra, sizeof(extra), "ROUND %d  %zu LEFT", m_search.rounds(), m_search.candidates().size());
        }
        draw_header(0, 0, w(), "CHANGED BYTES", extra);
    }

    void resize(int X, int Y, int W_, int H_) override
    {
        Fl_Double_Window::resize(X, Y, W_, H_);
        m_bar->resize(W_ - 14, m_list->y(), 14, H_ - m_list->y());
        m_list->resize(0, m_list->y(), W_ - 14, H_ - m_list->y());
    }

private:
    void apply()
    {
        const uint8_t* prev = nullptr;
        const uint8_t* cur = nullptr;
        if (!m_source.get || !m_source.get(&prev, &cur)) {
            status("No pause snapshot yet: pause the machine first.", COLOR_BREAK);
            return;
        }
        const Filter filter = (Filter) m_filter->value();
        uint8_t value = 0;
        if (filter == Filter::Equal && !parse_byte(m_value->value(), &value)) {
            status("Value: hex byte (3E) or decimal with # (#62)", COLOR_BREAK);
            return;
        }
        if (filter != Filter::Equal && prev == nullptr) {
            status("Needs two pauses: continue, then pause again.", COLOR_BREAK);
            return;
        }
        m_search.narrow(filter, value, prev, cur);
        m_list->scroll_to(0);
        update();
    }

    void update()
    {
        if (!m_search.active()) {
            status("Starts with all 65536 bytes; each Apply narrows the list.", COLOR_DIM);
            m_list->items = nullptr;
        } else {
            char t[96];
            snprintf(t, sizeof(t), "Round %d: %zu candidate%s. Click one to view it.", m_search.rounds(),
                     m_search.candidates().size(), m_search.candidates().size() == 1 ? "" : "s");
            status(t, COLOR_DIM);
            m_list->items = &m_search.candidates();
        }
        m_list->sync();
        redraw();
    }

    void update_filter()
    {
        if ((Filter) m_filter->value() == Filter::Equal) m_value->activate();
        else m_value->deactivate();
    }

    void status(const char* text, Fl_Color c)
    {
        m_status->copy_label(text);
        m_status->labelcolor(c);
        m_status->redraw();
    }

    static void cb_filter(Fl_Widget*, void* d) { static_cast<ChangeSearchWindow*>(d)->update_filter(); }
    static void cb_apply(Fl_Widget*, void* d) { static_cast<ChangeSearchWindow*>(d)->apply(); }
    static void cb_bar(Fl_Widget*, void* d)
    {
        ChangeSearchWindow* self = static_cast<ChangeSearchWindow*>(d);
        self->m_list->scroll_to(self->m_bar->value());
    }
    static void cb_reset(Fl_Widget*, void* d)
    {
        ChangeSearchWindow* self = static_cast<ChangeSearchWindow*>(d);
        self->m_search.stop();
        self->update();
    }

    SnapshotSource m_source;
    ChangeSearch m_search;
    Fl_Choice* m_filter;
    Fl_Input* m_value;
    Fl_Button* m_apply;
    Fl_Button* m_reset;
    Fl_Check_Button* m_auto;
    Fl_Box* m_status;
    Fl_Scrollbar* m_bar;
    CandidateList* m_list;
};

static ChangeSearchWindow* s_change_window;

void change_search_show(const SnapshotSource& source)
{
    if (s_change_window == nullptr) {
        s_change_window = new ChangeSearchWindow();
        s_change_window->source(source);
        MainWindow* mw = main_window();
        if (mw && mw->shown()) s_change_window->position(mw->x() + 40, mw->y() + 80);
    }
    s_change_window->show();
}

void change_search_paused()
{
    if (s_change_window) s_change_window->paused();
}

void change_search_destroy()
{
    delete s_change_window;
    s_change_window = nullptr;
}

} // namespace memtools
} // namespace zui

#endif /* CONFIG_ENABLE_DEBUGGER */
