/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Watch panel (main debugger window, right column): a list of memory
 * expressions shown with an explicit type, in hex and decimal columns.
 *
 * Expressions: hex address (C000, $C000, 0xC000), a symbol (score),
 * symbol/address plus or minus an offset (table+4: offsets are hex like
 * addresses, #10 is decimal), register indirection ((HL), (DE), (BC),
 * (SP), (IX+n), (IY-n)), or memory indirection ((ptr) = the word at ptr).
 * A ",N" suffix sets the length of a string watch (decimal).
 *
 * Pointer watches (*u8, *u16) read the little-endian word at the expression
 * and show the address it holds in the ADDR column, with the value found
 * there in the HEX/DEC columns. Bitfields show the byte in HEX and its 8
 * bits (7..0) as small 0/1 digits in the DEC column, set bits lit. */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include "panel.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

namespace {

enum WatchType { W_U8, W_S8, W_U16, W_S16, W_STR, W_PTR8, W_PTR16, W_BITS, W_COUNT };

struct TypeInfo {
    const char* key;   /* persisted name */
    const char* tag;   /* short tag drawn in the row and in the type choice */
    const char* label; /* context menu */
};

const TypeInfo s_types[W_COUNT] = {
    { "u8", "u8", "u8   unsigned byte" },
    { "s8", "s8", "s8   signed byte" },
    { "u16", "u16", "u16  unsigned word (LE)" },
    { "s16", "s16", "s16  signed word (LE)" },
    { "str", "str", "str  ASCII string" },
    { "ptr8", "*u8", "*u8  pointer to byte" },
    { "ptr16", "*u16", "*u16 pointer to word" },
    { "bits", "bits", "bits bitfield (8 bits)" },
};

constexpr int MAX_WATCHES = 16; /* persisted entries share rini's 128-value capacity */
constexpr int MAX_EXPR = 48;
constexpr int MAX_STR = 64;
constexpr int DEFAULT_STR = 16;
constexpr int MAX_BYTES = MAX_STR + 2;

struct Watch {
    std::string expr;
    int type = W_U8;
    int len = DEFAULT_STR;
    /* Evaluated at each refresh */
    bool valid = false;
    hwaddr addr = 0;
    hwaddr target = 0; /* pointer types: the address pointed to */
    uint8_t cur[MAX_BYTES] = {};
    int n = 0;
    /* Snapshot taken at the previous pause */
    bool has_prev = false;
    hwaddr prev_addr = 0;
    uint8_t prev[MAX_BYTES] = {};
    int prev_n = 0;
    bool changed = false;
    bool addr_changed = false;
    uint8_t bits_changed = 0; /* bitfields: bits that changed at the last pause */
};

/* The list lives outside the panel so that it can be saved at exit even
 * after the debugger window has been destroyed. */
std::vector<Watch>& watches()
{
    static std::vector<Watch> list;
    return list;
}

std::string trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char) s[b])) b++;
    while (e > b && isspace((unsigned char) s[e - 1])) e--;
    return s.substr(b, e - b);
}

/** Offset: hex by default ($/0x prefixes accepted), decimal with '#'. */
bool parse_offset(const std::string& text, long* out)
{
    std::string s = trim(text);
    int base = 16;
    size_t i = 0;
    if (!s.empty() && s[0] == '#') {
        base = 10;
        i = 1;
    } else if (!s.empty() && s[0] == '$') {
        i = 1;
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }
    if (i >= s.size()) return false;
    char* end = nullptr;
    const long v = strtol(s.c_str() + i, &end, base);
    if (end == s.c_str() + i || *end != '\0') return false;
    *out = v;
    return true;
}

void read_mem(hwaddr addr, int len, uint8_t* out)
{
    addr &= 0xFFFF;
    int first = 0x10000 - (int) addr;
    if (first > len) first = len;
    debugger_read_memory(fz_debugger(), addr, first, out);
    if (first < len) debugger_read_memory(fz_debugger(), 0, len - first, out + first);
}

/** Address designated by an expression (see the top of the file). */
bool eval_expr(const std::string& text, const regs_t& regs, hwaddr* out, int depth = 0)
{
    const std::string e = trim(text);
    if (e.empty() || depth > 3) return false;

    if (e.front() == '(' && e.back() == ')') {
        const std::string in = trim(e.substr(1, e.size() - 2));
        std::string up = in;
        for (char& c : up) c = (char) toupper((unsigned char) c);
        const struct {
            const char* name;
            uint16_t value;
        } regs16[] = {
            { "HL", regs.hl }, { "DE", regs.de }, { "BC", regs.bc },
            { "SP", regs.sp }, { "IX", regs.ix }, { "IY", regs.iy },
        };
        for (const auto& r : regs16) {
            if (up.compare(0, 2, r.name) != 0) continue;
            const std::string rest = trim(in.substr(2));
            long off = 0;
            if (!rest.empty()) {
                if (rest[0] != '+' && rest[0] != '-') break; /* not a register */
                if (!parse_offset(rest.substr(1), &off)) return false;
                if (rest[0] == '-') off = -off;
            }
            *out = (hwaddr) ((r.value + off) & 0xFFFF);
            return true;
        }
        /* Memory indirection: the little-endian word stored at the inner address */
        hwaddr ptr;
        if (!eval_expr(in, regs, &ptr, depth + 1)) return false;
        uint8_t b[2];
        read_mem(ptr, 2, b);
        *out = (hwaddr) (b[0] | (b[1] << 8));
        return true;
    }

    hwaddr addr;
    if (parse_address(e.c_str(), &addr)) {
        *out = addr;
        return true;
    }
    const size_t pos = e.find_last_of("+-");
    if (pos == std::string::npos || pos == 0) return false;
    long off;
    if (!parse_offset(e.substr(pos + 1), &off)) return false;
    if (!eval_expr(e.substr(0, pos), regs, &addr, depth + 1)) return false;
    if (e[pos] == '-') off = -off;
    *out = (hwaddr) ((addr + off) & 0xFFFF);
    return true;
}

void evaluate(Watch& w, const regs_t& regs)
{
    w.valid = eval_expr(w.expr, regs, &w.addr);
    if (!w.valid) {
        w.n = 0;
        return;
    }
    switch (w.type) {
        case W_U8:
        case W_S8:
        case W_BITS:
            w.n = 1;
            break;
        case W_U16:
        case W_S16:
            w.n = 2;
            break;
        case W_STR:
            w.n = w.len;
            break;
        case W_PTR8:
        case W_PTR16:
            w.n = 2;
            break;
        default:
            w.n = 1;
            break;
    }
    read_mem(w.addr, w.n, w.cur);
    if (w.type == W_PTR8 || w.type == W_PTR16) {
        w.target = (hwaddr) (w.cur[0] | (w.cur[1] << 8));
        const int tn = w.type == W_PTR8 ? 1 : 2;
        read_mem(w.target, tn, w.cur + 2);
        w.n = 2 + tn;
    }
}

bool is_pointer(const Watch& w)
{
    return w.type == W_PTR8 || w.type == W_PTR16;
}

/** Address shown in the ADDR column: where the displayed value lives
 *  (the target for pointers, whose own location is the expression). */
hwaddr shown_addr(const Watch& w)
{
    return is_pointer(w) ? w.target : w.addr;
}

/** Remember the current value as the reference for change highlighting. */
void take_snapshot(Watch& w)
{
    memcpy(w.prev, w.cur, sizeof(w.prev));
    w.prev_n = w.n;
    w.prev_addr = shown_addr(w);
    w.has_prev = w.valid;
}

/** HEX and DEC cell texts. Strings use `hex` only (quoted text). */
void format(const Watch& w, char* hex, size_t hs, char* dec, size_t ds)
{
    hex[0] = dec[0] = '\0';
    if (!w.valid) {
        snprintf(hex, hs, "?");
        return;
    }
    const unsigned b = w.cur[0];
    const unsigned word = w.cur[0] | (w.cur[1] << 8);
    switch (w.type) {
        case W_U8:
            hex8(hex, hs, b);
            snprintf(dec, ds, "%u", b);
            break;
        case W_S8:
            hex8(hex, hs, b);
            snprintf(dec, ds, "%d", (int) (int8_t) b);
            break;
        case W_U16:
            hex16(hex, hs, word);
            snprintf(dec, ds, "%u", word);
            break;
        case W_S16:
            hex16(hex, hs, word);
            snprintf(dec, ds, "%d", (int) (int16_t) word);
            break;
        case W_BITS:
            hex8(hex, hs, b); /* bits drawn by the list (draw_bits) */
            break;
        case W_PTR8: /* the pointer itself is shown in the ADDR column */
            hex8(hex, hs, w.cur[2]);
            snprintf(dec, ds, "%u", (unsigned) w.cur[2]);
            break;
        case W_PTR16: {
            const unsigned tw = w.cur[2] | (w.cur[3] << 8);
            hex16(hex, hs, tw);
            snprintf(dec, ds, "%u", tw);
            break;
        }
        case W_STR: {
            size_t o = 0;
            if (o + 1 < hs) hex[o++] = '"';
            for (int i = 0; i < w.n && o + 2 < hs; i++) {
                const uint8_t c = w.cur[i];
                if (c == 0) break; /* C string terminator */
                hex[o++] = (c >= 0x20 && c < 0x7F) ? (char) c : '.';
            }
            if (o + 1 < hs) hex[o++] = '"';
            hex[o] = '\0';
            break;
        }
        default:
            break;
    }
}

int type_from_key(const char* key)
{
    for (int i = 0; i < W_COUNT; i++) {
        if (strcmp(s_types[i].key, key) == 0) return i;
    }
    return W_U8;
}

/* ------------------------------------------------------------------ */
/*  Persistence                                                        */
/* ------------------------------------------------------------------ */

void watches_save(rini_config* ini)
{
    const std::vector<Watch>& list = watches();
    rini_set_config_comment_line(ini, "SDL/FLTK debugger watches (expr|type|length)");
    rini_set_config_value(ini, "FLTK_WATCH_COUNT", (int) list.size(), "Watch entries");
    char key[32], text[MAX_EXPR + 32];
    for (size_t i = 0; i < list.size(); i++) {
        snprintf(key, sizeof(key), "FLTK_WATCH_%d", (int) i);
        snprintf(text, sizeof(text), "%s|%s|%d", list[i].expr.c_str(), s_types[list[i].type].key, list[i].len);
        rini_set_config_value_text(ini, key, text, "Watch");
    }
}

void watches_load()
{
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    std::vector<Watch>& list = watches();
    int count = config_get("FLTK_WATCH_COUNT", 0);
    if (count > MAX_WATCHES) count = MAX_WATCHES;
    char key[32];
    for (int i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "FLTK_WATCH_%d", i);
        const char* value = config_get_text(key, nullptr);
        if (value == nullptr || value[0] == '\0') continue;
        const std::string s = value;
        Watch w;
        const size_t p1 = s.find('|');
        w.expr = trim(s.substr(0, p1));
        if (w.expr.empty() || w.expr.size() > MAX_EXPR) continue;
        if (p1 != std::string::npos) {
            const size_t p2 = s.find('|', p1 + 1);
            w.type = type_from_key(trim(s.substr(p1 + 1, p2 == std::string::npos ? std::string::npos : p2 - p1 - 1)).c_str());
            if (p2 != std::string::npos) {
                const int len = atoi(s.c_str() + p2 + 1);
                if (len >= 1 && len <= MAX_STR) w.len = len;
            }
        }
        list.push_back(w);
    }
    persist_register(watches_save);
}

/* ------------------------------------------------------------------ */
/*  List of watches (custom-drawn rows)                                */
/* ------------------------------------------------------------------ */

class WatchList : public Fl_Widget {
public:
    WatchList(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    /** Called after an edit of the list (count changed, type changed...). */
    std::function<void()> changed;

    static int head_h() { return 16; }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        if (!dbg_ready()) return;
        std::vector<Watch>& list = watches();
        const int lh = data_line_height();
        const int cw = char_width();
        const int asc = text_baseline(lh);
        fl_push_clip(x(), y(), w(), h());
        if (list.empty()) {
            fl_color(COLOR_DIM);
            fl_font(FL_HELVETICA, 11);
            fl_draw("No watches. Add an address, a symbol, table+4 or (HL) below.", x() + 8, y() + 4,
                    w() - 16, lh * 2, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
            fl_pop_clip();
            return;
        }

        /* Column widths (in characters) fit the widest value shown */
        char hex[MAX_STR + 8], dec[16];
        int hex_chars = 2, dec_chars = 3;
        for (const Watch& wt : list) {
            format(wt, hex, sizeof(hex), dec, sizeof(dec));
            if (wt.type != W_STR && (int) strlen(hex) > hex_chars) hex_chars = (int) strlen(hex);
            if ((int) strlen(dec) > dec_chars) dec_chars = (int) strlen(dec);
        }
        int dec_w = dec_chars * cw;
        for (const Watch& wt : list) {
            if (wt.type == W_BITS && bits_width() > dec_w) dec_w = bits_width();
        }
        const int right = x() + w() - 6;
        const int dec_x = right - dec_w;
        const int hex_x = dec_x - cw - hex_chars * cw;
        const int addr_x = hex_x - cw - 4 * cw;
        const int name_x = x() + 6;
        const int name_r = addr_x - cw;

        /* Column titles */
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        const int ty = y() + head_h() - 4;
        fl_draw("EXPR", name_x, ty);
        fl_draw("ADDR", addr_x, ty);
        fl_draw("HEX", hex_x, ty);
        fl_draw("DEC", right - (int) fl_width("DEC"), ty);
        fl_color(COLOR_BORDER);
        fl_xyline(x() + 4, y() + head_h() - 1, x() + w() - 4);

        clamp_first();
        const int rows = visible_rows();
        char text[16];
        for (int i = 0; i < rows && m_first + i < (int) list.size(); i++) {
            const int index = m_first + i;
            const Watch& wt = list[(size_t) index];
            const int ry = rows_y() + i * lh;
            if (index == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x(), ry, w(), lh);
            }
            /* Type tag, right-aligned in the name column */
            fl_font(FL_HELVETICA, 10);
            char tag[16];
            if (wt.type == W_STR) snprintf(tag, sizeof(tag), "str%d", wt.len);
            else snprintf(tag, sizeof(tag), "%s", s_types[wt.type].tag);
            const int tag_w = (int) fl_width(tag);
            fl_color(COLOR_DIM);
            fl_draw(tag, name_r - tag_w, ry + asc);
            /* Expression */
            fl_font(DATA_FONT, DATA_SIZE);
            fl_push_clip(name_x, ry, name_r - tag_w - 4 - name_x, lh);
            fl_color(COLOR_AMBER);
            fl_draw(wt.expr.c_str(), name_x, ry + asc);
            fl_pop_clip();
            /* Address */
            if (wt.valid) {
                hex16(text, sizeof(text), shown_addr(wt));
                fl_color(wt.addr_changed ? COLOR_CHANGED : COLOR_CYAN);
            } else {
                snprintf(text, sizeof(text), "????");
                fl_color(COLOR_DIM);
            }
            fl_draw(text, addr_x, ry + asc);
            /* Value */
            format(wt, hex, sizeof(hex), dec, sizeof(dec));
            const Fl_Color vc = !wt.valid ? COLOR_DIM : wt.changed ? COLOR_CHANGED : COLOR_TEXT;
            if (wt.type == W_STR && wt.valid) {
                fl_push_clip(hex_x, ry, right - hex_x, lh);
                fl_color(vc);
                fl_draw(hex, hex_x, ry + asc);
                fl_pop_clip();
            } else {
                fl_color(vc);
                fl_draw(hex, hex_x, ry + asc);
                fl_draw(dec, right - (int) strlen(dec) * cw, ry + asc);
                if (wt.type == W_BITS && wt.valid) draw_bits(wt, right - bits_width(), ry + asc);
            }
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
                m_first += Fl::event_dy();
                clamp_first();
                set_hover(row_at(Fl::event_y()));
                if (changed) changed(); /* header shows the scroll offset */
                redraw();
                return 1; /* never let the wheel fall through */
            case FL_PUSH: {
                if (!dbg_ready()) return 1;
                const int r = row_at(Fl::event_y());
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    context_menu(r);
                } else if (r >= 0 && watches()[(size_t) r].valid) {
                    goto_memory(shown_addr(watches()[(size_t) r]));
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

    /** Scroll so that the last entry is visible (after adding one). */
    void show_last()
    {
        m_first = (int) watches().size() - visible_rows();
        clamp_first();
    }

    int first() const { return m_first; }

private:
    static constexpr int BITS_FONT = 11;

    /** Bitfield digits use the small UI font to fit the DEC column. */
    static int bits_width()
    {
        fl_font(FL_HELVETICA, BITS_FONT);
        const int wd = (int) fl_width("0") * 8 + 3;
        fl_font(DATA_FONT, DATA_SIZE);
        return wd;
    }

    /** Bits 7..0 as 0/1, a small gap between the nibbles; set bits lit,
     *  bits that changed since the last pause in the change colour. */
    static void draw_bits(const Watch& wt, int bx, int baseline)
    {
        fl_font(FL_HELVETICA, BITS_FONT);
        const int dw = (int) fl_width("0");
        const uint8_t b = wt.cur[0];
        const uint8_t diff = wt.changed ? wt.bits_changed : 0;
        for (int i = 0; i < 8; i++) {
            const uint8_t mask = (uint8_t) (0x80 >> i);
            fl_color((diff & mask) ? COLOR_CHANGED : (b & mask) ? COLOR_GREEN : COLOR_DIM);
            fl_draw((b & mask) ? "1" : "0", bx + i * dw + (i >= 4 ? 3 : 0), baseline);
        }
        fl_font(DATA_FONT, DATA_SIZE);
    }

    int rows_y() const { return y() + head_h() + 2; }

    int visible_rows() const
    {
        const int r = (h() - head_h() - 2) / data_line_height();
        return r > 0 ? r : 0;
    }

    void clamp_first()
    {
        int max_first = (int) watches().size() - visible_rows();
        if (max_first < 0) max_first = 0;
        if (m_first > max_first) m_first = max_first;
        if (m_first < 0) m_first = 0;
    }

    int row_at(int ey) const
    {
        if (ey < rows_y()) return -1;
        const int i = (ey - rows_y()) / data_line_height();
        if (i >= visible_rows()) return -1;
        const int r = m_first + i;
        return r < (int) watches().size() ? r : -1;
    }

    void set_hover(int r)
    {
        if (r == m_hover) return;
        m_hover = r;
        redraw();
    }

    void context_menu(int row)
    {
        std::vector<Watch>& list = watches();
        const bool has_row = row >= 0;
        const Watch* wt = has_row ? &list[(size_t) row] : nullptr;
        const int row_only = has_row ? 0 : FL_MENU_INACTIVE;
        const int valid = (wt && wt->valid) ? 0 : FL_MENU_INACTIVE;
        const bool ptr = wt && is_pointer(*wt);
        const int any = list.empty() ? FL_MENU_INACTIVE : 0;
        auto radio = [&](int t) {
            return FL_MENU_RADIO | ((wt && wt->type == t) ? FL_MENU_VALUE : 0);
        };
        enum {
            M_TYPE = 0,
            M_TYPE_FIRST = 1,
            M_TYPE_END = M_TYPE_FIRST + W_COUNT,
            M_LEN = M_TYPE_END + 1,
            M_MEM,
            M_DIS,
            M_TARGET,
            M_UP,
            M_DOWN,
            M_REMOVE,
            M_REMOVE_ALL,
        };
        Fl_Menu_Item items[] = {
            { "Change Type", 0, nullptr, nullptr, FL_SUBMENU | row_only, 0, 0, 0, 0 },
            { s_types[0].label, 0, nullptr, nullptr, radio(0), 0, 0, 0, 0 },
            { s_types[1].label, 0, nullptr, nullptr, radio(1), 0, 0, 0, 0 },
            { s_types[2].label, 0, nullptr, nullptr, radio(2), 0, 0, 0, 0 },
            { s_types[3].label, 0, nullptr, nullptr, radio(3), 0, 0, 0, 0 },
            { s_types[4].label, 0, nullptr, nullptr, radio(4), 0, 0, 0, 0 },
            { s_types[5].label, 0, nullptr, nullptr, radio(5), 0, 0, 0, 0 },
            { s_types[6].label, 0, nullptr, nullptr, radio(6), 0, 0, 0, 0 },
            { s_types[7].label, 0, nullptr, nullptr, radio(7), 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
            { "String Length...", 0, nullptr, nullptr,
              (wt && wt->type == W_STR) ? FL_MENU_DIVIDER : (FL_MENU_INACTIVE | FL_MENU_DIVIDER), 0, 0, 0, 0 },
            { "Show in Memory", 0, nullptr, nullptr, valid, 0, 0, 0, 0 },
            { "Show in Disassembly", 0, nullptr, nullptr, valid, 0, 0, 0, 0 },
            { "Show Pointer in Memory", 0, nullptr, nullptr,
              (ptr && wt->valid ? 0 : FL_MENU_INACTIVE) | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Move Up", 0, nullptr, nullptr, row > 0 ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
            { "Move Down", 0, nullptr, nullptr,
              has_row && row + 1 < (int) list.size() ? 0 : FL_MENU_INACTIVE, 0, 0, 0, 0 },
            { "Remove", 0, nullptr, nullptr, row_only | FL_MENU_DIVIDER, 0, 0, 0, 0 },
            { "Remove All", 0, nullptr, nullptr, any, 0, 0, 0, 0 },
            { nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0 },
        };
        static_assert(sizeof(items) / sizeof(items[0]) == M_REMOVE_ALL + 2, "menu layout");
        const Fl_Menu_Item* picked = items->popup(Fl::event_x(), Fl::event_y());
        if (picked == nullptr) return;
        const int id = (int) (picked - items);
        if (id >= M_TYPE_FIRST && id < M_TYPE_END) {
            list[(size_t) row].type = id - M_TYPE_FIRST;
            list[(size_t) row].has_prev = false;
        } else {
            switch (id) {
                case M_LEN: {
                    char def[8];
                    snprintf(def, sizeof(def), "%d", list[(size_t) row].len);
                    const char* answer = fl_input("String length in characters (1-64, decimal):", def);
                    if (answer == nullptr) return;
                    const int len = atoi(answer);
                    if (len < 1 || len > MAX_STR) {
                        fl_beep();
                        return;
                    }
                    list[(size_t) row].len = len;
                    list[(size_t) row].has_prev = false;
                    break;
                }
                case M_MEM: goto_memory(shown_addr(list[(size_t) row])); return;
                case M_DIS: goto_disassembly(shown_addr(list[(size_t) row])); return;
                case M_TARGET: goto_memory(list[(size_t) row].addr); return;
                case M_UP: std::swap(list[(size_t) row], list[(size_t) row - 1]); break;
                case M_DOWN: std::swap(list[(size_t) row], list[(size_t) row + 1]); break;
                case M_REMOVE: list.erase(list.begin() + row); break;
                case M_REMOVE_ALL: list.clear(); break;
                default: return;
            }
        }
        m_hover = -1;
        clamp_first();
        if (changed) changed();
    }

    int m_hover = -1;
    int m_first = 0; /* first entry shown (mouse wheel) */
};


/* ------------------------------------------------------------------ */
/*  Panel                                                              */
/* ------------------------------------------------------------------ */

class WatchPanel : public Panel {
public:
    WatchPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H, "WATCH")
    {
        watches_load();
        m_list = new WatchList(X, Y, 10, 10);
        m_list->tooltip("Click: show in memory    Right-click: change type, move, remove...");
        m_list->changed = [this] { update(); };
        m_input = new Fl_Input(X, Y, 10, 10);
        style_input(m_input);
        m_input->maximum_size(MAX_EXPR + 4);
        m_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        m_input->callback(cb_add, this);
        m_input->tooltip("C000, $C000, symbol, table+4 (hex; #4 = decimal), (HL), (IX+2), (ptr)\n"
                         "Optional ,N sets a string's length. Enter adds.");
        m_type = new Fl_Choice(X, Y, 10, 10);
        for (int i = 0; i < W_COUNT; i++) m_type->add(s_types[i].tag);
        m_type->value(W_U8);
        m_type->textsize(12);
        m_type->clear_visible_focus();
        m_type->tooltip("Display type of the new watch");
        m_add = new Fl_Button(X, Y, 10, 10, "Add");
        style_button(m_add);
        m_add->callback(cb_add, this);
        resizable(m_list);
        end();
        resize(X, Y, W, H);
        update_extra();
    }

    void refresh(bool paused, bool force) override
    {
        if (!dbg_ready()) return;
        const regs_t regs = current_regs();
        for (Watch& w : watches()) {
            evaluate(w, regs);
            if (paused && force) {
                w.changed = w.has_prev && w.valid &&
                            (w.n != w.prev_n || memcmp(w.cur, w.prev, (size_t) w.n) != 0);
                w.addr_changed = w.has_prev && w.valid && shown_addr(w) != w.prev_addr;
                w.bits_changed = w.changed ? (uint8_t) (w.cur[0] ^ w.prev[0]) : 0;
                take_snapshot(w);
            } else if (!paused) {
                w.changed = w.addr_changed = false;
            }
        }
        update_extra();
        m_list->redraw();
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Widget::resize(X, Y, W, H);
        const int iy = Y + H - BAR_H + 4;
        m_list->resize(X, Y + HEADER_H, W, H - HEADER_H - BAR_H);
        m_input->resize(X + 6, iy, W - 6 - 72 - 50 - 4, BAR_H - 8);
        m_type->resize(X + W - 50 - 72, iy, 68, BAR_H - 8);
        m_add->resize(X + W - 50, iy, 44, BAR_H - 8);
    }

private:
    /** Re-evaluate after a local edit; keeps the change snapshot. */
    void update()
    {
        if (dbg_ready()) {
            const regs_t regs = current_regs();
            for (Watch& w : watches()) {
                evaluate(w, regs);
                /* Entries without a reference value (new, retyped) take the
                 * current one, so that they only light up on real changes. */
                if (!w.has_prev) take_snapshot(w);
            }
        }
        update_extra();
        m_list->redraw();
    }

    void update_extra()
    {
        char text[32];
        const int n = (int) watches().size();
        if (m_list && m_list->first() > 0) snprintf(text, sizeof(text), "%d  +%d", n, m_list->first());
        else snprintf(text, sizeof(text), "%d", n);
        extra(text);
    }

    static void cb_add(Fl_Widget*, void* data)
    {
        WatchPanel* self = static_cast<WatchPanel*>(data);
        std::vector<Watch>& list = watches();
        /* Spaces are never significant; '|' and '"' would break the saved line */
        std::string expr;
        for (const char* p = self->m_input->value(); *p; p++) {
            if (!isspace((unsigned char) *p)) expr += *p;
        }
        Watch w;
        w.type = self->m_type->value();
        if (w.type < 0 || w.type >= W_COUNT) w.type = W_U8;
        const size_t comma = expr.rfind(',');
        if (comma != std::string::npos) {
            const std::string n = expr.substr(comma + 1);
            const int len = atoi(n.c_str());
            if (n.empty() || n.find_first_not_of("0123456789") != std::string::npos || len < 1 || len > MAX_STR) {
                fl_beep();
                return;
            }
            w.len = len;
            expr.resize(comma);
        }
        w.expr = expr;
        hwaddr addr;
        if (!dbg_ready() || expr.empty() || expr.size() > MAX_EXPR ||
            expr.find_first_of("|\"") != std::string::npos || (int) list.size() >= MAX_WATCHES ||
            !eval_expr(expr, current_regs(), &addr)) {
            fl_beep();
            return;
        }
        list.push_back(w);
        self->m_input->value("");
        self->m_list->show_last();
        self->update();
    }

    WatchList* m_list = nullptr;
    Fl_Input* m_input = nullptr;
    Fl_Choice* m_type = nullptr;
    Fl_Button* m_add = nullptr;
};

} // namespace

Panel* create_watch_panel(int x, int y, int w, int h)
{
    return new WatchPanel(x, y, w, h);
}

#endif

} // namespace zui
