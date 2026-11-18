/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Input devices as the machine sees them (Hardware window): PS/2 keyboard
 * (held keys, scancodes sent, controller FIFO) and the SNES adapter ports. */

#include <cstdio>
#include <cstring>
#include <FL/fl_draw.H>
#include "tool_window.h"

extern "C" {
#include "frontend/sdl-fltk/bridge_input.h"
#include "hw/keys.h"
}

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

/* ------------------------------------------------------------------ */
/*  Keyboard glyph layout: 15 units main block + 3 units navigation    */
/* ------------------------------------------------------------------ */

struct GlyphKey {
    const char* label;
    uint16_t code;   /* 0: gap */
    float units;
};

#define K(l, c, u) { l, (uint16_t) (c), u }
#define GAP(u)     { nullptr, 0, u }

static const GlyphKey s_row0[] = {
    K("Esc", ZEAL_KEY_ESCAPE, 1), GAP(1),
    K("F1", ZEAL_KEY_F1, 1), K("F2", ZEAL_KEY_F2, 1), K("F3", ZEAL_KEY_F3, 1), K("F4", ZEAL_KEY_F4, 1), GAP(0.5f),
    K("F5", ZEAL_KEY_F5, 1), K("F6", ZEAL_KEY_F6, 1), K("F7", ZEAL_KEY_F7, 1), K("F8", ZEAL_KEY_F8, 1), GAP(0.5f),
    K("F9", ZEAL_KEY_F9, 1), K("F10", ZEAL_KEY_F10, 1), K("F11", ZEAL_KEY_F11, 1), K("F12", ZEAL_KEY_F12, 1),
    GAP(0.5f), K("Prt", ZEAL_KEY_PRINT_SCREEN, 1), K("Scr", ZEAL_KEY_SCROLL_LOCK, 1), K("Pau", ZEAL_KEY_PAUSE, 1),
};
static const GlyphKey s_row1[] = {
    K("`", ZEAL_KEY_GRAVE, 1), K("1", '1', 1), K("2", '2', 1), K("3", '3', 1), K("4", '4', 1), K("5", '5', 1),
    K("6", '6', 1), K("7", '7', 1), K("8", '8', 1), K("9", '9', 1), K("0", '0', 1), K("-", '-', 1),
    K("=", '=', 1), K("Bksp", ZEAL_KEY_BACKSPACE, 2),
    GAP(0.5f), K("Ins", ZEAL_KEY_INSERT, 1), K("Hom", ZEAL_KEY_HOME, 1), K("PgU", ZEAL_KEY_PAGE_UP, 1),
};
static const GlyphKey s_row2[] = {
    K("Tab", ZEAL_KEY_TAB, 1.5f), K("Q", 'Q', 1), K("W", 'W', 1), K("E", 'E', 1), K("R", 'R', 1), K("T", 'T', 1),
    K("Y", 'Y', 1), K("U", 'U', 1), K("I", 'I', 1), K("O", 'O', 1), K("P", 'P', 1), K("[", '[', 1),
    K("]", ']', 1), K("\\", '\\', 1.5f),
    GAP(0.5f), K("Del", ZEAL_KEY_DELETE, 1), K("End", ZEAL_KEY_END, 1), K("PgD", ZEAL_KEY_PAGE_DOWN, 1),
};
static const GlyphKey s_row3[] = {
    K("Caps", ZEAL_KEY_CAPS_LOCK, 1.75f), K("A", 'A', 1), K("S", 'S', 1), K("D", 'D', 1), K("F", 'F', 1),
    K("G", 'G', 1), K("H", 'H', 1), K("J", 'J', 1), K("K", 'K', 1), K("L", 'L', 1), K(";", ';', 1),
    K("'", '\'', 1), K("Enter", ZEAL_KEY_ENTER, 2.25f),
};
static const GlyphKey s_row4[] = {
    K("Shift", ZEAL_KEY_LEFT_SHIFT, 2.25f), K("Z", 'Z', 1), K("X", 'X', 1), K("C", 'C', 1), K("V", 'V', 1),
    K("B", 'B', 1), K("N", 'N', 1), K("M", 'M', 1), K(",", ',', 1), K(".", '.', 1), K("/", '/', 1),
    K("Shift", ZEAL_KEY_RIGHT_SHIFT, 2.75f), GAP(1.5f), K("^", ZEAL_KEY_UP, 1),
};
static const GlyphKey s_row5[] = {
    K("Ctl", ZEAL_KEY_LEFT_CONTROL, 1.25f), K("Sup", ZEAL_KEY_LEFT_SUPER, 1.25f), K("Alt", ZEAL_KEY_LEFT_ALT, 1.25f),
    K("", ZEAL_KEY_SPACE, 6.25f), K("Alt", ZEAL_KEY_RIGHT_ALT, 1.25f), K("Sup", ZEAL_KEY_RIGHT_SUPER, 1.25f),
    K("Mnu", ZEAL_KEY_KB_MENU, 1.25f), K("Ctl", ZEAL_KEY_RIGHT_CONTROL, 1.25f),
    GAP(0.5f), K("<", ZEAL_KEY_LEFT, 1), K("v", ZEAL_KEY_DOWN, 1), K(">", ZEAL_KEY_RIGHT, 1),
};

#undef K
#undef GAP

struct GlyphRow {
    const GlyphKey* keys;
    int count;
};

static const GlyphRow s_rows[] = {
    { s_row0, (int) (sizeof(s_row0) / sizeof(s_row0[0])) },
    { s_row1, (int) (sizeof(s_row1) / sizeof(s_row1[0])) },
    { s_row2, (int) (sizeof(s_row2) / sizeof(s_row2[0])) },
    { s_row3, (int) (sizeof(s_row3) / sizeof(s_row3[0])) },
    { s_row4, (int) (sizeof(s_row4) / sizeof(s_row4[0])) },
    { s_row5, (int) (sizeof(s_row5) / sizeof(s_row5[0])) },
};
constexpr float GLYPH_UNITS = 18.5f;
constexpr int GLYPH_ROWS = 6;

static const char* s_snes_names[12] = { "B", "Y", "Select", "Start", "Up", "Down",
                                        "Left", "Right", "A", "X", "L", "R" };

/* ------------------------------------------------------------------ */

class InputView : public Panel {
public:
    InputView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "INPUT")
    {
        tooltip("Input devices as the emulated machine sees them");
        end();
        Fl::add_timeout(0.1, cb_tick, this);
    }

    ~InputView() override
    {
        Fl::remove_timeout(cb_tick, this);
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        update();
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        draw_header(x(), y(), w(), "INPUT", extra_text());

        const bool two_cols = w() >= 720;
        const int pad = 10;
        const int left_w = two_cols ? (int) (w() * 0.56) : w();
        const int body_y = y() + HEADER_H + 6;

        fl_push_clip(x(), body_y, left_w, h() - (body_y - y()));
        draw_keyboard(x() + pad, body_y, left_w - 2 * pad, y() + h());
        fl_pop_clip();

        if (two_cols) {
            fl_color(COLOR_BORDER);
            fl_yxline(x() + left_w, body_y, y() + h() - 6);
            fl_push_clip(x() + left_w + 1, body_y, w() - left_w - 1, h() - (body_y - y()));
            draw_snes(x() + left_w + pad, body_y, w() - left_w - 2 * pad, y() + h());
            fl_pop_clip();
        } else {
            fl_color(COLOR_DIM);
            fl_font(FL_HELVETICA, 11);
            fl_draw("Widen the window to see the SNES adapter", x() + pad, y() + h() - 8);
        }
    }

    int handle(int event) override
    {
        if (event == FL_MOUSEWHEEL) return 1;
        return Panel::handle(event);
    }

private:
    void update()
    {
        char text[64];
        snprintf(text, sizeof(text), "%u scancodes  %u latches", (unsigned) fz_input_scancode_total(),
                 (unsigned) fz_input_snes_latches());
        extra(text);
        redraw();
    }

    static void cb_tick(void* data)
    {
        InputView* self = (InputView*) data;
        /* Held keys and pads change quickly: redraw at 10 Hz while running */
        if (self->visible_r() && !fz_paused()) self->update();
        Fl::repeat_timeout(0.1, cb_tick, data);
    }

    /* -------------------------------------------------------------- */

    static int section(int X, int Y, int W, const char* title, const char* right = nullptr)
    {
        fl_font(FL_HELVETICA_BOLD, 10);
        fl_color(COLOR_AMBER);
        fl_draw(title, X, Y + 11);
        if (right) {
            fl_font(FL_HELVETICA, 10);
            fl_color(COLOR_DIM);
            fl_draw(right, X + W - (int) fl_width(right), Y + 11);
        }
        fl_color(COLOR_BORDER);
        fl_xyline(X, Y + 15, X + W);
        return Y + 20;
    }

    /** Small rectangle chip; returns its width. */
    static int chip(int X, int Y, int H, const char* label, bool lit, Fl_Font font, int size)
    {
        fl_font(font, size);
        const int cw = (int) fl_width(label) + 8;
        if (lit) {
            fl_color(COLOR_GREEN);
            fl_rectf(X, Y, cw, H);
            fl_color(COLOR_BG);
        } else {
            fl_color(COLOR_BORDER);
            fl_rect(X, Y, cw, H);
            fl_color(COLOR_DIM);
        }
        fl_draw(label, X + 4, Y + (H + size) / 2 - 2);
        return cw;
    }

    static bool held(const fz_keyboard_state_t& kb, uint16_t code)
    {
        for (int i = 0; i < kb.held_count; i++) {
            if (kb.held[i] == code) return true;
        }
        return false;
    }

    void draw_keyboard(int X, int Y, int W, int bottom)
    {
        fz_keyboard_state_t kb;
        fz_input_keyboard(&kb);
        int yy = section(X, Y, W, "PS/2 KEYBOARD");

        /* Keyboard glyph */
        float u = W / GLYPH_UNITS;
        if (u > 26) u = 26;
        const int kh = (int) u - 2;
        for (int r = 0; r < GLYPH_ROWS; r++) {
            float fx = 0;
            const int ry = yy + (int) (r * u) + (r > 0 ? 4 : 0);
            for (int i = 0; i < s_rows[r].count; i++) {
                const GlyphKey& k = s_rows[r].keys[i];
                const int kx = X + (int) (fx * u);
                const int kw = (int) (k.units * u) - 2;
                fx += k.units;
                if (k.label == nullptr) continue;
                const bool lit = held(kb, k.code);
                if (lit) {
                    fl_color(COLOR_GREEN);
                    fl_rectf(kx, ry, kw, kh);
                } else {
                    fl_color(COLOR_RAISED);
                    fl_rectf(kx, ry, kw, kh);
                    fl_color(COLOR_BORDER);
                    fl_rect(kx, ry, kw, kh);
                }
                fl_font(FL_HELVETICA, u >= 22 ? 9 : 8);
                fl_color(lit ? COLOR_BG : COLOR_DIM);
                const int lw = (int) fl_width(k.label);
                if (lw < kw) fl_draw(k.label, kx + (kw - lw) / 2, ry + kh / 2 + 3);
            }
        }
        yy += (int) (GLYPH_ROWS * u) + 12;

        /* Held keys */
        const int lh = data_line_height();
        const int asc = text_baseline(lh);
        const int cw = char_width();
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(COLOR_DIM);
        fl_draw("HELD", X, yy + asc);
        int cx = X + cw * 6;
        if (kb.held_count == 0) {
            fl_font(DATA_FONT, DATA_SIZE);
            fl_color(COLOR_DIM);
            fl_draw("-", cx, yy + asc);
        }
        for (int i = 0; i < kb.held_count && cx < X + W - 40; i++) {
            cx += chip(cx, yy, lh, fz_key_name(kb.held[i]), true, DATA_FONT, DATA_SIZE) + 4;
        }
        yy += lh + 4;

        /* Controller FIFO and shift register */
        char text[160];
        char hex[8];
        fl_font(DATA_FONT, DATA_SIZE);
        fl_color(COLOR_DIM);
        fl_draw("FIFO", X, yy + asc);
        snprintf(text, sizeof(text), "%d/%d", kb.fifo_count, kb.fifo_size);
        fl_color(kb.fifo_count ? COLOR_TEXT : COLOR_DIM);
        fl_draw(text, X + cw * 6, yy + asc);
        int n = 0;
        text[0] = '\0';
        const int fit = (W - cw * 16) / (cw * 3);
        for (int i = 0; i < kb.fifo_count && i < FZ_KB_FIFO_SHOWN && i < fit; i++) {
            hex8(hex, sizeof(hex), kb.fifo[i]);
            n += snprintf(text + n, sizeof(text) - (size_t) n, "%s ", hex);
        }
        if (kb.fifo_count > fit && fit > 0 && n < (int) sizeof(text) - 4) snprintf(text + n, sizeof(text) - (size_t) n, "...");
        fl_color(COLOR_CYAN);
        fl_draw(text, X + cw * 16, yy + asc);
        yy += lh;

        static const char* states[] = { "IDLE", "START BIT", "GAP" };
        fl_color(COLOR_DIM);
        fl_draw("SHIFT", X, yy + asc);
        hex8(hex, sizeof(hex), kb.shift_register);
        snprintf(text, sizeof(text), "$%s", hex);
        fl_color(COLOR_CYAN);
        fl_draw(text, X + cw * 6, yy + asc);
        fl_color(COLOR_DIM);
        fl_draw("PIN", X + cw * 12, yy + asc);
        fl_color(kb.pin ? COLOR_TEXT : COLOR_GREEN);
        fl_draw(kb.pin ? "1" : "0", X + cw * 16, yy + asc);
        fl_color(kb.state == FZ_PS2_IDLE ? COLOR_DIM : COLOR_GREEN);
        fl_draw(states[kb.state], X + cw * 19, yy + asc);
        yy += lh + 10;

        /* Scancode log */
        yy = section(X, yy, W, "KEY EVENTS SENT", "newest first");
        fz_scancode_t log[FZ_SCANCODE_LOG];
        const int count = fz_input_scancodes(log, FZ_SCANCODE_LOG);
        /* Legend: MAKE/BREAK are the PS/2 names for key down/up codes */
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_GREEN);
        fl_draw("MAKE", X, yy + asc);
        fl_color(COLOR_DIM);
        fl_draw("= key down", X + (int) fl_width("MAKE "), yy + asc);
        int lx = X + (int) fl_width("MAKE = key down    ");
        fl_color(COLOR_AMBER);
        fl_draw("REPEAT", lx, yy + asc);
        fl_color(COLOR_DIM);
        fl_draw("= held, auto-repeat", lx + (int) fl_width("REPEAT "), yy + asc);
        lx += (int) fl_width("REPEAT = held, auto-repeat    ");
        fl_color(COLOR_TEXT);
        fl_draw("BREAK", lx, yy + asc);
        fl_color(COLOR_DIM);
        fl_draw("= key up (F0 prefix)", lx + (int) fl_width("BREAK "), yy + asc);
        yy += lh + 4;
        fl_font(FL_HELVETICA_BOLD, 10);
        fl_color(COLOR_DIM);
        fl_draw("GAP (ms)", X, yy + asc);
        fl_draw("EVENT", X + cw * 12, yy + asc);
        fl_draw("KEY", X + cw * 21, yy + asc);
        fl_draw("PS/2 BYTES", X + cw * 30, yy + asc);
        yy += lh;
        fl_font(DATA_FONT, DATA_SIZE);
        if (count == 0) {
            fl_color(COLOR_DIM);
            fl_draw("No key events yet", X, yy + asc);
        }
        for (int i = 0; i < count && yy + lh <= bottom - 4; i++) {
            const fz_scancode_t& e = log[i];
            /* Gap to the previous (older) event: hold time, repeat rate */
            if (i + 1 < count) {
                const double ms = (double) (e.time_ns - log[i + 1].time_ns) / 1e6;
                if (ms < 10000.0) snprintf(text, sizeof(text), "%+9.1f", ms);
                else snprintf(text, sizeof(text), "%9s", "> 10 s");
            } else {
                snprintf(text, sizeof(text), "%9s", "-");
            }
            fl_color(COLOR_DIM);
            fl_draw(text, X, yy + asc);
            fl_color(e.make ? (e.repeat ? COLOR_AMBER : COLOR_GREEN) : COLOR_TEXT);
            fl_draw(e.make ? (e.repeat ? "REPEAT" : "MAKE") : "BREAK", X + cw * 12, yy + asc);
            fl_color(COLOR_TEXT);
            fl_draw(fz_key_name(e.key), X + cw * 21, yy + asc);
            n = 0;
            text[0] = '\0';
            for (int b = 0; b < e.count; b++) {
                hex8(hex, sizeof(hex), e.bytes[b]);
                n += snprintf(text + n, sizeof(text) - (size_t) n, "%s ", hex);
            }
            fl_color(COLOR_CYAN);
            fl_draw(text, X + cw * 30, yy + asc);
            yy += lh;
        }
    }

    /* -------------------------------------------------------------- */

    /** SNES pad outline with lit buttons; returns its height. */
    static int draw_pad(int X, int Y, uint16_t pressed, bool available)
    {
        const int W = 176, H = 72;
        const Fl_Color body = available ? COLOR_RAISED : COLOR_SURFACE;
        auto lit = [pressed](int bit) { return ((pressed >> bit) & 1) != 0; };
        auto fill = [&](bool on) { fl_color(on ? COLOR_GREEN : COLOR_BG); };

        /* Shoulders */
        fill(lit(FZ_SNES_L));
        fl_rectf(X + 14, Y, 40, 6);
        fill(lit(FZ_SNES_R));
        fl_rectf(X + W - 54, Y, 40, 6);
        /* Body: two lobes joined by a bar */
        const int by = Y + 6;
        const int bh = H - 6;
        fl_color(body);
        fl_pie(X, by, bh, bh, 0, 360);
        fl_pie(X + W - bh, by, bh, bh, 0, 360);
        fl_rectf(X + bh / 2, by, W - bh, bh);
        fl_color(COLOR_BORDER);
        fl_arc(X, by, bh, bh, 90, 270);
        fl_arc(X + W - bh, by, bh, bh, -90, 90);
        fl_xyline(X + bh / 2, by, X + W - bh / 2);
        fl_xyline(X + bh / 2, by + bh - 1, X + W - bh / 2);

        /* D-pad */
        const int dx = X + bh / 2, dy = by + bh / 2, s = 9;
        fill(lit(FZ_SNES_UP));
        fl_rectf(dx - s / 2, dy - s / 2 - s, s, s);
        fill(lit(FZ_SNES_DOWN));
        fl_rectf(dx - s / 2, dy + s / 2 + 1, s, s);
        fill(lit(FZ_SNES_LEFT));
        fl_rectf(dx - s / 2 - s, dy - s / 2, s, s);
        fill(lit(FZ_SNES_RIGHT));
        fl_rectf(dx + s / 2 + 1, dy - s / 2, s, s);
        fl_color(COLOR_BG);
        fl_rectf(dx - s / 2, dy - s / 2, s + 1, s);

        /* Select / Start */
        fill(lit(FZ_SNES_SELECT));
        fl_rectf(X + W / 2 - 22, dy + 2, 16, 5);
        fill(lit(FZ_SNES_START));
        fl_rectf(X + W / 2 + 6, dy + 2, 16, 5);

        /* Face buttons: X top, B bottom, Y left, A right */
        const int fx = X + W - bh / 2, fy = dy, r = 7, d = 12;
        struct { int bit; int ox; int oy; const char* label; } faces[] = {
            { FZ_SNES_X, 0, -d, "X" }, { FZ_SNES_B, 0, d, "B" },
            { FZ_SNES_Y, -d, 0, "Y" }, { FZ_SNES_A, d, 0, "A" },
        };
        fl_font(FL_HELVETICA_BOLD, 8);
        for (auto& f : faces) {
            const bool on = lit(f.bit);
            fill(on);
            fl_pie(fx + f.ox - r, fy + f.oy - r, 2 * r, 2 * r, 0, 360);
            fl_color(on ? COLOR_BG : COLOR_DIM);
            fl_draw(f.label, fx + f.ox - (int) fl_width(f.label) / 2, fy + f.oy + 3);
        }
        return H;
    }

    void draw_snes(int X, int Y, int W, int bottom)
    {
        const int lh = data_line_height();
        const int asc = text_baseline(lh);
        const int cw = char_width();
        char text[128];
        char hex[8];

        const uint64_t last = fz_input_snes_last_latch_ns();
        const uint32_t latches = fz_input_snes_latches();
        if (latches == 0) snprintf(text, sizeof(text), "never latched");
        else snprintf(text, sizeof(text), "last latch %.2f s ago", (double) (fz_emulated_ns() - last) / 1e9);
        int yy = section(X, Y, W, "SNES ADAPTER", text);

        fz_snes_mouse_t mouse;
        fz_input_snes_mouse(&mouse);
        for (int port = 0; port < 2; port++) {
            fz_snes_port_t p;
            fz_input_snes_port(port, &p);
            fl_font(DATA_FONT, DATA_SIZE);
            snprintf(text, sizeof(text), "PORT %d", port + 1);
            fl_color(COLOR_TEXT);
            fl_draw(text, X, yy + asc);
            if (p.device == FZ_SNES_PAD) {
                snprintf(text, sizeof(text), "pad %d  %s", p.gamepad, p.name ? p.name : "(disconnected)");
            } else if (p.device == FZ_SNES_MOUSE) {
                snprintf(text, sizeof(text), "mouse");
            } else {
                snprintf(text, sizeof(text), "nothing attached");
            }
            fl_color(p.device == FZ_SNES_NONE ? COLOR_DIM : p.available ? COLOR_CYAN : COLOR_DIM);
            fl_draw(text, X + cw * 8, yy + asc);
            yy += lh + 2;

            /* Raw latch: 16 bits for a pad, 32 for the mouse (active low) */
            fl_color(COLOR_DIM);
            fl_draw("LATCH", X, yy + asc);
            if (p.device == FZ_SNES_MOUSE) {
                hex16(hex, sizeof(hex), p.latched >> 16);
                char lo[8];
                hex16(lo, sizeof(lo), p.latched & 0xFFFF);
                snprintf(text, sizeof(text), "$%s%s", hex, lo);
            } else {
                hex16(hex, sizeof(hex), p.latched & 0xFFFF);
                snprintf(text, sizeof(text), "$%s", hex);
            }
            fl_color(p.device == FZ_SNES_NONE ? COLOR_DIM : COLOR_CYAN);
            fl_draw(text, X + cw * 8, yy + asc);
            yy += lh + 4;

            if (p.device == FZ_SNES_PAD) {
                yy += draw_pad(X, yy, p.pressed, p.available) + 6;
                /* Pressed buttons by name */
                fl_font(DATA_FONT, DATA_SIZE);
                int cx = X;
                bool any = false;
                for (int b = 0; b < 12; b++) {
                    if (!((p.pressed >> b) & 1)) continue;
                    any = true;
                    if (cx > X + W - 60) break;
                    cx += chip(cx, yy, lh, s_snes_names[b], true, DATA_FONT, DATA_SIZE) + 4;
                }
                if (!any) {
                    fl_color(COLOR_DIM);
                    fl_draw("no buttons pressed", X, yy + asc);
                }
                yy += lh + 10;
            } else if (p.device == FZ_SNES_MOUSE) {
                int cx = X;
                cx += chip(cx, yy, lh, "LEFT", mouse.left, DATA_FONT, DATA_SIZE) + 4;
                cx += chip(cx, yy, lh, "RIGHT", mouse.right, DATA_FONT, DATA_SIZE) + 10;
                fl_font(DATA_FONT, DATA_SIZE);
                fl_color(mouse.captured ? COLOR_GREEN : COLOR_DIM);
                fl_draw(mouse.captured ? "CAPTURED" : mouse.hovered ? "hovered" : "not captured", cx, yy + asc);
                yy += lh + 4;
                static const char* speeds[] = { "slow", "medium", "fast", "?" };
                if (mouse.latched) snprintf(text, sizeof(text), "last dx %+d  dy %+d", mouse.dx, mouse.dy);
                else snprintf(text, sizeof(text), "not latched yet");
                fl_color(mouse.latched ? COLOR_TEXT : COLOR_DIM);
                fl_draw(text, X, yy + asc);
                yy += lh;
                snprintf(text, sizeof(text), "speed %s  host scale x%.2f",
                         mouse.latched ? speeds[mouse.speed & 3] : "-", (double) mouse.scale);
                fl_color(COLOR_DIM);
                fl_draw(text, X, yy + asc);
                yy += lh + 10;
            } else {
                yy += 6;
            }
            if (yy > bottom) return;
        }

        /* Host gamepads */
        yy = section(X, yy, W, "HOST GAMEPADS", "slot  port  name");
        fl_font(DATA_FONT, DATA_SIZE);
        bool any = false;
        for (int i = 0; i < fz_snes_gamepad_count() && yy + lh <= bottom; i++) {
            const char* name = fz_snes_gamepad_name(i);
            if (name == nullptr) continue;
            any = true;
            const int port = fz_snes_gamepad_port(i);
            snprintf(text, sizeof(text), "%d", i);
            fl_color(COLOR_CYAN);
            fl_draw(text, X, yy + asc);
            if (port >= 0) snprintf(text, sizeof(text), "%d", port + 1); else snprintf(text, sizeof(text), "-");
            fl_color(port >= 0 ? COLOR_TEXT : COLOR_DIM);
            fl_draw(text, X + cw * 6, yy + asc);
            fl_color(fz_snes_gamepad_available(i) ? COLOR_TEXT : COLOR_DIM);
            fl_draw(name, X + cw * 12, yy + asc);
            yy += lh;
        }
        if (!any) {
            fl_color(COLOR_DIM);
            fl_draw("No host gamepad connected", X, yy + asc);
        }
    }
};

static Panel* make_input(int x, int y, int w, int h)
{
    return new InputView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_HARDWARE, 40, "Input", make_input);

#endif

} // namespace zui
