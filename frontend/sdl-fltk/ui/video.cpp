/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* "Video & Profiler" tool window. Graphics work happens in a different
 * phase than stepping code, so these viewers get their own window (as in
 * Mesen) that can sit beside the debugger or on a second monitor:
 *
 *   [Layer 0][Layer 1][Tileset][Palette][Font][Profiler]        zoom [2x]
 *   +------------------------------------------+--------------------+
 *   | VRAM view (scrollable, zoomable)         | INSPECTOR          |
 *   |                                          |  magnified cell    |
 *   |                                          |  index / colour    |
 *   +------------------------------------------+--------------------+
 */

#include <cstdio>
#include <cstring>
#include <vector>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_draw.H>
#include "ui.h"
#include "video.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

enum { VIEW_SPRITES = DBG_VIEW_TOTAL, VIEW_PROFILER };
constexpr int TOP_H = 38;
constexpr int INSPECTOR_W = 270;

static const char* s_view_names[] = { "Layer 0", "Layer 1", "Tileset", "Palette", "Font", "Sprites", "Profiler" };

/* ------------------------------------------------------------------ */
/*  Segmented view switcher                                            */
/* ------------------------------------------------------------------ */

class Segment : public Fl_Button {
public:
    Segment(int X, int Y, int W, int H, const char* label) : Fl_Button(X, Y, W, H, label)
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

/* ------------------------------------------------------------------ */
/*  VRAM image and inspector                                           */
/* ------------------------------------------------------------------ */

class Inspector;

class VramImage : public Fl_Widget {
public:
    VramImage(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}

    Inspector* inspector = nullptr;
    fz_image_t img = {};
    bool valid = false;
    int zoom = 2;
    dbg_vram_t view = DBG_TILEMAP_LAYER0;
    int hover_cell = -1;
    int pinned_cell = -1;

    void update(dbg_vram_t v, int z)
    {
        if (v != view) {
            hover_cell = pinned_cell = -1;
        }
        view = v;
        zoom = z;
        valid = fz_video_mode() != FZ_VIDEO_BITMAP && fz_vram_render(v, &img);
        if (!valid) {
            size(420, 60);
            redraw();
            return;
        }
        const int W = img.width * zoom;
        const int H = img.height * zoom;
        m_scaled.resize((size_t) W * H * 3);
        for (int yy = 0; yy < H; yy++) {
            const uint8_t* src = img.pixels + (size_t) (yy / zoom) * img.width * 3;
            uint8_t* dst = &m_scaled[(size_t) yy * W * 3];
            for (int xx = 0; xx < W; xx++) {
                const uint8_t* p = src + (xx / zoom) * 3;
                *dst++ = p[0];
                *dst++ = p[1];
                *dst++ = p[2];
            }
        }
        if (w() != W || h() != H) {
            size(W, H);
            if (parent()) parent()->redraw();
        }
        redraw();
    }

    int cell_count() const
    {
        if (!valid) return 0;
        return img.columns * (img.height / (img.cell_height + 1));
    }

    void draw() override;
    int handle(int event) override;

private:
    int cell_at(int ex, int ey) const
    {
        if (!valid) return -1;
        const int px = (ex - x()) / zoom;
        const int py = (ey - y()) / zoom;
        const int cx = px / (img.cell_width + 1);
        const int cy = py / (img.cell_height + 1);
        if (px < 0 || py < 0 || cx >= img.columns) return -1;
        const int cell = cy * img.columns + cx;
        return cell < cell_count() ? cell : -1;
    }

    std::vector<uint8_t> m_scaled;
};

class Inspector : public Fl_Widget {
public:
    Inspector(int X, int Y, int W, int H, VramImage* image) : Fl_Widget(X, Y, W, H), m_image(image) {}

    int handle(int event) override
    {
        return event == FL_MOUSEWHEEL ? 1 : Fl_Widget::handle(event);
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        const int cell = m_image->pinned_cell >= 0 ? m_image->pinned_cell : m_image->hover_cell;
        draw_header(x(), y(), w(), "INSPECTOR", m_image->pinned_cell >= 0 ? "PINNED" : "");
        const fz_image_t& img = m_image->img;
        fl_font(FL_HELVETICA, 12);
        if (!m_image->valid) {
            fl_color(COLOR_DIM);
            fl_draw("The VRAM views are not available in bitmap modes.", x() + 12, y() + HEADER_H + 8,
                    w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
            return;
        }
        if (cell < 0) {
            fl_color(COLOR_DIM);
            fl_draw("Hover a cell to inspect it.\nClick to pin, click again to unpin.", x() + 12, y() + HEADER_H + 8,
                    w() - 24, 60, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
            return;
        }

        /* Magnified cell, including its grid border */
        const int cw = img.cell_width + 2;
        const int ch = img.cell_height + 2;
        const int cx = cell % img.columns;
        const int cy = cell / img.columns;
        const int sx0 = cx * (img.cell_width + 1);
        const int sy0 = cy * (img.cell_height + 1);
        int scale = (w() - 24) / cw;
        if (scale * ch > 200) scale = 200 / ch;
        if (scale < 1) scale = 1;
        const int bx = x() + (w() - cw * scale) / 2;
        const int by = y() + HEADER_H + 12;
        for (int yy = 0; yy < ch; yy++) {
            for (int xx = 0; xx < cw; xx++) {
                const int sx = sx0 + xx, sy = sy0 + yy;
                if (sx >= img.width || sy >= img.height) continue;
                const uint8_t* p = img.pixels + ((size_t) sy * img.width + sx) * 3;
                fl_color(fl_rgb_color(p[0], p[1], p[2]));
                fl_rectf(bx + xx * scale, by + yy * scale, scale, scale);
            }
        }
        fl_color(COLOR_BORDER);
        fl_rect(bx - 1, by - 1, cw * scale + 2, ch * scale + 2);

        /* Details */
        int ty = by + ch * scale + 24;
        char line[64];
        auto row = [&](const char* label, const char* value, Fl_Color c) {
            fl_font(FL_HELVETICA, 11);
            fl_color(COLOR_DIM);
            fl_draw(label, x() + 12, ty);
            fl_font(DATA_FONT, DATA_SIZE);
            fl_color(c);
            fl_draw(value, x() + 90, ty);
            ty += data_line_height() + 2;
        };
        switch (m_image->view) {
            case DBG_TILEMAP_LAYER0:
            case DBG_TILEMAP_LAYER1:
                snprintf(line, sizeof(line), "%d, %d", cx, cy);
                row("CELL", line, COLOR_TEXT);
                snprintf(line, sizeof(line), "%d", cell);
                row("INDEX", line, COLOR_TEXT);
                snprintf(line, sizeof(line), config.debugger.hex_upper ? "$%04X" : "$%04x", cell);
                row("OFFSET", line, COLOR_CYAN);
                break;
            case DBG_TILESET:
                snprintf(line, sizeof(line), config.debugger.hex_upper ? "%d ($%03X)" : "%d ($%03x)", cell, cell);
                row("TILE", line, COLOR_TEXT);
                break;
            case DBG_PALETTE: {
                const int px = sx0 + 1 + img.cell_width / 2;
                const int py = sy0 + 1 + img.cell_height / 2;
                const uint8_t* p = img.pixels + ((size_t) py * img.width + px) * 3;
                snprintf(line, sizeof(line), "%d ($%02X)", cell, cell);
                row("COLOR", line, COLOR_TEXT);
                snprintf(line, sizeof(line), "#%02X%02X%02X", p[0], p[1], p[2]);
                row("RGB", line, COLOR_CYAN);
                const unsigned rgb565 = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
                snprintf(line, sizeof(line), "$%04X", rgb565);
                row("RGB565", line, COLOR_TEXT);
                break;
            }
            case DBG_FONT:
                snprintf(line, sizeof(line), "%d ($%02X)", cell, cell);
                row("CHAR", line, COLOR_TEXT);
                if (cell >= 32 && cell < 127) {
                    snprintf(line, sizeof(line), "'%c'", cell);
                    row("ASCII", line, COLOR_AMBER);
                }
                break;
            default:
                break;
        }
    }

private:
    VramImage* m_image;
};

void VramImage::draw()
{
    if (!valid) {
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        fl_color(COLOR_DIM);
        fl_font(FL_HELVETICA, 12);
        fl_draw("Not available in bitmap mode", x() + 12, y() + 24);
        return;
    }
    fl_draw_image(m_scaled.data(), x(), y(), w(), h(), 3);
    for (int sel : { hover_cell, pinned_cell }) {
        if (sel < 0) continue;
        const int cx = sel % img.columns;
        const int cy = sel / img.columns;
        fl_color(sel == pinned_cell ? COLOR_AMBER : COLOR_CYAN);
        fl_rect(x() + cx * (img.cell_width + 1) * zoom, y() + cy * (img.cell_height + 1) * zoom,
                (img.cell_width + 2) * zoom, (img.cell_height + 2) * zoom);
    }
}

int VramImage::handle(int event)
{
    switch (event) {
        case FL_ENTER:
            return 1;
        case FL_LEAVE:
            hover_cell = -1;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        case FL_MOVE: {
            const int cell = cell_at(Fl::event_x(), Fl::event_y());
            if (cell != hover_cell) {
                hover_cell = cell;
                redraw();
                if (inspector) inspector->redraw();
            }
            return 1;
        }
        case FL_PUSH: {
            const int cell = cell_at(Fl::event_x(), Fl::event_y());
            pinned_cell = (cell == pinned_cell) ? -1 : cell;
            redraw();
            if (inspector) inspector->redraw();
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}


/* ------------------------------------------------------------------ */
/*  Sprites: sheet of all 128 hardware sprites + inspector             */
/* ------------------------------------------------------------------ */

struct SpriteData {
    fz_sprite_t info[FZ_SPRITE_COUNT] = {};
    uint8_t rgb[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H * 3] = {};
    uint8_t mask[FZ_SPRITE_COUNT][FZ_SPRITE_W * FZ_SPRITE_MAX_H] = {};
    bool gfx = false;
    int screen_w = 640;
    int screen_h = 480;
    int hover = -1;
    int selected = -1;

    int current() const { return selected >= 0 ? selected : hover; }

    void update()
    {
        gfx = fz_gfx_screen(&screen_w, &screen_h);
        for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
            if (!fz_sprite(i, &info[i], rgb[i], mask[i])) memset(&info[i], 0, sizeof(info[i]));
        }
    }
};

/** Compose a sprite over a checkerboard, scaled, into an RGB buffer. */
static void compose_sprite(const SpriteData& d, int index, int zoom, bool dim, std::vector<uint8_t>& out)
{
    const int W = FZ_SPRITE_W * zoom;
    const int H = FZ_SPRITE_MAX_H * zoom;
    out.resize((size_t) W * H * 3);
    const int check = zoom * 4;
    const int height = d.info[index].height;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const int sx = x / zoom, sy = y / zoom;
            uint8_t r, g, b;
            if (sy >= height) {
                r = g = b = 0x18; /* outside a 16px-high sprite */
            } else if (d.mask[index][sy * FZ_SPRITE_W + sx]) {
                const uint8_t* p = &d.rgb[index][(sy * FZ_SPRITE_W + sx) * 3];
                r = p[0]; g = p[1]; b = p[2];
            } else {
                const bool light = ((x / check) + (y / check)) & 1;
                r = g = b = light ? 0x30 : 0x26;
            }
            if (dim) { r /= 3; g /= 3; b /= 3; }
            uint8_t* o = &out[((size_t) y * W + x) * 3];
            o[0] = r; o[1] = g; o[2] = b;
        }
    }
}

class SpriteSheet : public Fl_Widget {
public:
    static constexpr int COLUMNS = 8;

    SpriteSheet(int X, int Y, int W, int H, SpriteData* data) : Fl_Widget(X, Y, W, H), m_data(data)
    {
        tooltip("Hover to inspect, click to pin. Dimmed sprites are off-screen.");
    }

    Fl_Widget* inspector = nullptr;
    int zoom = 2;

    int card_w() const { return FZ_SPRITE_W * zoom + 14; }
    int card_h() const { return FZ_SPRITE_MAX_H * zoom + 30; }

    void relayout()
    {
        const int W = COLUMNS * card_w() + 8;
        const int H = (FZ_SPRITE_COUNT / COLUMNS) * card_h() + 8 + (m_data->gfx ? 0 : 28);
        if (w() != W || h() != H) {
            size(W, H);
            if (parent()) parent()->redraw();
        }
        redraw();
    }

    void draw() override
    {
        fl_color(COLOR_BG);
        fl_rectf(x(), y(), w(), h());
        int top = y() + 4;
        if (!m_data->gfx) {
            fl_font(FL_HELVETICA, 12);
            fl_color(COLOR_AMBER);
            fl_draw("Text mode: sprites are only displayed in graphics modes.", x() + 8, top + 16);
            top += 28;
        }
        const int lh = data_line_height();
        for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
            const int cx = x() + 4 + (i % COLUMNS) * card_w();
            const int cy = top + (i / COLUMNS) * card_h();
            if (cx + card_w() < x() || cy > y() + h()) continue;
            const fz_sprite_t& sp = m_data->info[i];
            const bool off = !sp.on_screen;
            fl_color(i == m_data->hover ? COLOR_HOVER : COLOR_SURFACE);
            fl_rectf(cx + 2, cy + 2, card_w() - 4, card_h() - 4);
            compose_sprite(*m_data, i, zoom, off, m_buf);
            fl_draw_image(m_buf.data(), cx + 7, cy + 7, FZ_SPRITE_W * zoom, FZ_SPRITE_MAX_H * zoom, 3);
            if (i == m_data->selected || i == m_data->hover) {
                fl_color(i == m_data->selected ? COLOR_AMBER : COLOR_CYAN);
                fl_rect(cx + 2, cy + 2, card_w() - 4, card_h() - 4);
            }
            char label[8];
            snprintf(label, sizeof(label), "%d", i);
            fl_font(DATA_FONT, DATA_SIZE);
            fl_color(off ? COLOR_DIM : COLOR_CYAN);
            fl_draw(label, cx + 7, cy + 7 + FZ_SPRITE_MAX_H * zoom + lh - 2);
        }
    }

    int handle(int event) override
    {
        switch (event) {
            case FL_ENTER:
                return 1;
            case FL_LEAVE:
                m_data->hover = -1;
                redraw();
                if (inspector) inspector->redraw();
                return 1;
            case FL_MOVE: {
                const int i = sprite_at(Fl::event_x(), Fl::event_y());
                if (i != m_data->hover) {
                    m_data->hover = i;
                    redraw();
                    if (inspector) inspector->redraw();
                }
                return 1;
            }
            case FL_PUSH: {
                const int i = sprite_at(Fl::event_x(), Fl::event_y());
                m_data->selected = (i == m_data->selected) ? -1 : i;
                redraw();
                if (inspector) inspector->redraw();
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    int sprite_at(int ex, int ey) const
    {
        const int top = y() + 4 + (m_data->gfx ? 0 : 28);
        if (ex < x() + 4 || ey < top) return -1;
        const int col = (ex - x() - 4) / card_w();
        const int row = (ey - top) / card_h();
        if (col >= COLUMNS) return -1;
        const int i = row * COLUMNS + col;
        return i < FZ_SPRITE_COUNT ? i : -1;
    }

    SpriteData* m_data;
    std::vector<uint8_t> m_buf;
};

class SpriteInspector : public Fl_Widget {
public:
    SpriteInspector(int X, int Y, int W, int H, SpriteData* data) : Fl_Widget(X, Y, W, H), m_data(data) {}

    Fl_Widget* sheet = nullptr;

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        const int cur = m_data->current();
        char extra[24] = "";
        if (cur >= 0) snprintf(extra, sizeof(extra), "#%d%s", cur, m_data->selected >= 0 ? "  PINNED" : "");
        draw_header(x(), y(), w(), "SPRITE", extra);

        /* Screen map: where every on-screen sprite sits */
        map_rect(m_mx, m_my, m_mw, m_mh);
        fl_color(COLOR_BG);
        fl_rectf(m_mx, m_my, m_mw, m_mh);
        fl_color(COLOR_BORDER);
        fl_rect(m_mx - 1, m_my - 1, m_mw + 2, m_mh + 2);
        fl_font(FL_HELVETICA, 10);
        fl_color(COLOR_DIM);
        char res[24];
        snprintf(res, sizeof(res), "%dx%d", m_data->screen_w, m_data->screen_h);
        fl_draw(res, m_mx + 3, m_my + m_mh - 4);
        fl_push_clip(m_mx, m_my, m_mw, m_mh);
        for (int pass = 0; pass < 2; pass++) {
            for (int i = FZ_SPRITE_COUNT - 1; i >= 0; i--) {
                const fz_sprite_t& sp = m_data->info[i];
                if (!sp.on_screen) continue;
                const bool is_cur = i == cur;
                if ((pass == 1) != is_cur) continue;
                int rx, ry, rw, rh;
                sprite_rect(sp, rx, ry, rw, rh);
                fl_color(is_cur ? COLOR_AMBER : COLOR_CYAN);
                if (is_cur) fl_rectf(rx, ry, rw, rh); else fl_rect(rx, ry, rw, rh);
            }
        }
        fl_pop_clip();

        int ty = m_my + m_mh + 14;
        fl_font(FL_HELVETICA, 12);
        if (cur < 0) {
            fl_color(COLOR_DIM);
            fl_draw("Hover a sprite, or click one in the\nsheet or on the map to pin it.", x() + 12, ty,
                    w() - 24, 40, FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
            return;
        }

        /* Magnified sprite on the left, attributes on the right */
        const fz_sprite_t& sp = m_data->info[cur];
        const int scale = 4;
        compose_sprite(*m_data, cur, scale, false, m_buf);
        fl_draw_image(m_buf.data(), x() + 12, ty, FZ_SPRITE_W * scale, FZ_SPRITE_MAX_H * scale, 3);
        fl_color(COLOR_BORDER);
        fl_rect(x() + 11, ty - 1, FZ_SPRITE_W * scale + 2, FZ_SPRITE_MAX_H * scale + 2);

        const bool up = config.debugger.hex_upper;
        const int ax = x() + 12 + FZ_SPRITE_W * scale + 14;
        int ay = ty + 10;
        char v[48];
        auto row = [&](const char* label, const char* value, Fl_Color c) {
            fl_font(FL_HELVETICA, 10);
            fl_color(COLOR_DIM);
            fl_draw(label, ax, ay);
            fl_font(DATA_FONT, DATA_SIZE);
            fl_color(c);
            fl_draw(value, ax + 52, ay);
            ay += data_line_height() + 1;
        };
        snprintf(v, sizeof(v), up ? "$%03X %d" : "$%03x %d", sp.x, sp.screen_x);
        row("X", v, COLOR_TEXT);
        snprintf(v, sizeof(v), up ? "$%03X %d" : "$%03x %d", sp.y, sp.screen_y);
        row("Y", v, COLOR_TEXT);
        snprintf(v, sizeof(v), "%d%s", sp.tile, sp.bank ? " B1" : "");
        row("TILE", v, COLOR_CYAN);
        snprintf(v, sizeof(v), "%d", sp.palette);
        row("PAL", v, COLOR_TEXT);
        snprintf(v, sizeof(v), "16x%d", sp.height);
        row("SIZE", v, COLOR_TEXT);
        snprintf(v, sizeof(v), "%s%s", sp.flip_x ? "X " : "", sp.flip_y ? "Y" : (sp.flip_x ? "" : "-"));
        row("FLIP", v, COLOR_TEXT);
        row("PRIO", sp.behind_fg ? "behind" : "front", sp.behind_fg ? COLOR_DIM : COLOR_TEXT);
        snprintf(v, sizeof(v), up ? "%04X %04X" : "%04x %04x", sp.raw_flags, sp.raw_extra);
        row("RAW", v, COLOR_DIM);
        row("STATE", sp.on_screen ? "on screen" : "off screen", sp.on_screen ? COLOR_GREEN : COLOR_AMBER);
    }

    int handle(int event) override
    {
        if (event == FL_MOUSEWHEEL) return 1;
        if (event == FL_PUSH) {
            map_rect(m_mx, m_my, m_mw, m_mh);
            const int ex = Fl::event_x(), ey = Fl::event_y();
            if (ex >= m_mx && ey >= m_my && ex < m_mx + m_mw && ey < m_my + m_mh) {
                /* Lowest index wins, as in the hardware priority order */
                for (int i = 0; i < FZ_SPRITE_COUNT; i++) {
                    const fz_sprite_t& sp = m_data->info[i];
                    if (!sp.on_screen) continue;
                    int rx, ry, rw, rh;
                    sprite_rect(sp, rx, ry, rw, rh);
                    if (ex >= rx && ey >= ry && ex < rx + rw && ey < ry + rh) {
                        m_data->selected = i;
                        redraw();
                        if (sheet) sheet->redraw();
                        return 1;
                    }
                }
                m_data->selected = -1;
                redraw();
                if (sheet) sheet->redraw();
            }
            return 1;
        }
        return Fl_Widget::handle(event);
    }

private:
    void map_rect(int& mx, int& my, int& mw, int& mh) const
    {
        mx = x() + 12;
        my = y() + HEADER_H + 10;
        mw = w() - 24;
        mh = mw * 3 / 4;
    }

    void sprite_rect(const fz_sprite_t& sp, int& rx, int& ry, int& rw, int& rh) const
    {
        const double sx = (double) m_mw / m_data->screen_w;
        const double sy = (double) m_mh / m_data->screen_h;
        rx = m_mx + (int) (sp.screen_x * sx);
        ry = m_my + (int) (sp.screen_y * sy);
        rw = (int) (FZ_SPRITE_W * sx) > 2 ? (int) (FZ_SPRITE_W * sx) : 2;
        rh = (int) (sp.height * sy) > 2 ? (int) (sp.height * sy) : 2;
    }

    SpriteData* m_data;
    std::vector<uint8_t> m_buf;
    int m_mx = 0, m_my = 0, m_mw = 0, m_mh = 0;
};

/* ------------------------------------------------------------------ */
/*  Profiler (semihost counters)                                       */
/* ------------------------------------------------------------------ */

class ProfilerView : public Fl_Widget {
public:
    ProfilerView(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H)
    {
        tooltip("Click a counter to toggle break-on-update");
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        draw_header(x(), y(), w(), "SEMIHOST COUNTERS", "* running   ! break on update");
        const int lh = data_line_height() + 4;
        const int cols[] = { 12, 70, 300, 520 };
        int yy = y() + HEADER_H + 10;
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        const char* heads[] = { "#", "MICROSECONDS  min / avg / max", "MILLISECONDS  min / avg / max", "TOTAL  us / ms" };
        for (int i = 0; i < 4; i++) fl_draw(heads[i], x() + cols[i], yy + 12);
        yy += 22;
        fl_color(COLOR_BORDER);
        fl_xyline(x() + 8, yy - 4, x() + w() - 8);
        fl_font(DATA_FONT, DATA_SIZE);
        const int asc = lh - fl_descent() - (lh - fl_height()) / 2;
        char text[96];
        m_rows_y = yy;
        m_row_h = lh;
        for (int i = 0; i < fz_semihost_counter_count(); i++) {
            fz_counter_t c;
            if (!fz_semihost_counter(i, &c)) continue;
            const int ry = yy + i * lh;
            if (i == m_hover) {
                fl_color(COLOR_HOVER);
                fl_rectf(x() + 4, ry, w() - 8, lh);
            }
            if (c.break_on_update) {
                fl_color(COLOR_BREAK);
                fl_rectf(x() + 4, ry, 3, lh);
            }
            snprintf(text, sizeof(text), "%d%s%s", i, c.running ? "*" : "", c.break_on_update ? "!" : "");
            fl_color(c.running ? COLOR_GREEN : c.break_on_update ? COLOR_BREAK : COLOR_CYAN);
            fl_draw(text, x() + cols[0], ry + asc);
            fl_color(COLOR_TEXT);
            snprintf(text, sizeof(text), "%llu / %llu / %llu", (unsigned long long) c.min_us,
                     (unsigned long long) c.avg_us, (unsigned long long) c.max_us);
            fl_draw(text, x() + cols[1], ry + asc);
            snprintf(text, sizeof(text), "%llu / %llu / %llu", (unsigned long long) c.min_us / 1000,
                     (unsigned long long) c.avg_us / 1000, (unsigned long long) c.max_us / 1000);
            fl_draw(text, x() + cols[2], ry + asc);
            fl_color(COLOR_AMBER);
            snprintf(text, sizeof(text), "%llu / %llu", (unsigned long long) c.total_us,
                     (unsigned long long) c.total_us / 1000);
            fl_draw(text, x() + cols[3], ry + asc);
        }
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
                const int r = row_at(Fl::event_y());
                if (r != m_hover) {
                    m_hover = r;
                    redraw();
                }
                return 1;
            }
            case FL_MOUSEWHEEL:
                return 1;
            case FL_PUSH: {
                const int r = row_at(Fl::event_y());
                if (r >= 0) {
                    fz_semihost_toggle_break(r);
                    redraw();
                }
                return 1;
            }
            default:
                break;
        }
        return Fl_Widget::handle(event);
    }

private:
    int row_at(int ey) const
    {
        if (m_row_h <= 0 || ey < m_rows_y) return -1;
        const int r = (ey - m_rows_y) / m_row_h;
        return r < fz_semihost_counter_count() ? r : -1;
    }

    int m_rows_y = 0;
    int m_row_h = 0;
    int m_hover = -1;
};

/* ------------------------------------------------------------------ */
/*  Window                                                             */
/* ------------------------------------------------------------------ */

class VideoWindow : public Fl_Double_Window {
public:
    VideoWindow() : Fl_Double_Window(980, 660, FZ_WIN_NAME " - Video & Profiler")
    {
        color(COLOR_BG);
        int sx = 8;
        for (int i = 0; i <= VIEW_PROFILER; i++) {
            const int sw = i == VIEW_PROFILER ? 84 : 76;
            Segment* s = new Segment(sx, 6, sw, TOP_H - 10, s_view_names[i]);
            s->callback(cb_view, (void*) (intptr_t) i);
            if (i == 0) s->setonly();
            sx += sw + (i == VIEW_SPRITES ? 14 : 2);
        }
        m_zoom = new Fl_Choice(w() - 90, 8, 80, TOP_H - 14);
        m_zoom->add("1x|2x|3x|4x");
        m_zoom->value(1);
        m_zoom->textsize(12);
        m_zoom->clear_visible_focus();
        m_zoom->callback(cb_zoom, this);
        m_zoom->tooltip("Zoom");

        m_scroll = new Fl_Scroll(0, TOP_H, w() - INSPECTOR_W - 4, h() - TOP_H);
        m_scroll->box(FL_FLAT_BOX);
        m_scroll->color(COLOR_BG);
        m_image = new VramImage(0, TOP_H, 10, 10);
        m_sheet = new SpriteSheet(0, TOP_H, 10, 10, &m_sprites);
        m_sheet->hide();
        m_scroll->end();
        m_inspector = new Inspector(w() - INSPECTOR_W, TOP_H, INSPECTOR_W, h() - TOP_H, m_image);
        m_image->inspector = m_inspector;
        m_sprite_inspector = new SpriteInspector(w() - INSPECTOR_W, TOP_H, INSPECTOR_W, h() - TOP_H, &m_sprites);
        m_sprite_inspector->hide();
        m_sheet->inspector = m_sprite_inspector;
        m_sprite_inspector->sheet = m_sheet;
        m_profiler = new ProfilerView(0, TOP_H, w(), h() - TOP_H);
        m_profiler->hide();
        resizable(m_scroll);
        end();
        size_range(520, 320);
        /* A tool window: stays above the debugger, closes with it */
        set_non_modal();
        callback(cb_close);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Double_Window::resize(X, Y, W, H);
        m_zoom->resize(W - 90, 8, 80, TOP_H - 14);
        m_scroll->resize(0, TOP_H, W - INSPECTOR_W - 4, H - TOP_H);
        m_inspector->resize(W - INSPECTOR_W, TOP_H, INSPECTOR_W, H - TOP_H);
        m_sprite_inspector->resize(W - INSPECTOR_W, TOP_H, INSPECTOR_W, H - TOP_H);
        m_profiler->resize(0, TOP_H, W, H - TOP_H);
    }

    void draw() override
    {
        Fl_Double_Window::draw();
        fl_color(COLOR_BORDER);
        fl_xyline(0, TOP_H - 1, w());
    }

    void refresh(bool paused, bool force)
    {
        if (!shown()) return;
        const double now = now_seconds();
        if (!force && (paused || now - m_last < 0.25)) return;
        m_last = now;
        if (m_view == VIEW_PROFILER) {
            m_profiler->redraw();
        } else if (m_view == VIEW_SPRITES) {
            m_sprites.update();
            m_sheet->zoom = m_zoom->value() + 1;
            m_sheet->relayout();
            m_scroll->redraw();
            m_sprite_inspector->redraw();
        } else {
            m_image->update((dbg_vram_t) m_view, m_zoom->value() + 1);
            m_scroll->redraw();
            m_inspector->redraw();
        }
    }

    void set_view(int view)
    {
        m_view = view;
        const bool profiler = view == VIEW_PROFILER;
        const bool sprites = view == VIEW_SPRITES;
        if (profiler) {
            m_scroll->hide();
            m_inspector->hide();
            m_sprite_inspector->hide();
            m_zoom->hide();
            m_profiler->show();
        } else {
            m_profiler->hide();
            m_scroll->show();
            m_zoom->show();
            if (sprites) {
                m_image->hide();
                m_inspector->hide();
                m_sheet->show();
                m_sprite_inspector->show();
            } else {
                m_sheet->hide();
                m_sprite_inspector->hide();
                m_image->show();
                m_inspector->show();
            }
            m_scroll->scroll_to(0, 0);
        }
        refresh(false, true);
        redraw();
    }

private:
    static void cb_view(Fl_Widget* w, void* data)
    {
        static_cast<VideoWindow*>(w->window())->set_view((int) (intptr_t) data);
    }

    static void cb_zoom(Fl_Widget*, void* data)
    {
        static_cast<VideoWindow*>(data)->refresh(false, true);
    }

    static void cb_close(Fl_Widget*, void*)
    {
        video_window_show(false);
        if (main_window()) main_window()->update_menus();
    }

    int m_view = DBG_TILEMAP_LAYER0;
    double m_last = 0;
    Fl_Choice* m_zoom;
    Fl_Scroll* m_scroll;
    VramImage* m_image;
    Inspector* m_inspector;
    SpriteData m_sprites;
    SpriteSheet* m_sheet;
    SpriteInspector* m_sprite_inspector;
    ProfilerView* m_profiler;
};

static VideoWindow* s_window;

void video_window_show(bool show)
{
    if (!show) {
        if (s_window && s_window->shown()) {
            video_window_capture_config();
            s_window->hide();
        }
        g_layout.video_shown = false;
        return;
    }
    if (s_window == nullptr) {
        s_window = new VideoWindow();
        const LayoutConfig& c = g_layout;
        if (c.video_w > 200 && c.video_h > 150) {
            s_window->resize(c.video_x >= 0 ? c.video_x : s_window->x(),
                             c.video_y >= 0 ? c.video_y : s_window->y(), c.video_w, c.video_h);
        }
    }
    g_layout.video_shown = true;
    s_window->show();
    s_window->refresh(fz_paused(), true);
}

void video_window_suspend()
{
    if (s_window && s_window->shown()) {
        video_window_capture_config();
        s_window->hide();
    }
}

bool video_window_shown()
{
    return s_window && s_window->shown();
}

void video_window_refresh(bool paused, bool force)
{
    if (s_window) s_window->refresh(paused, force);
}

void video_window_capture_config()
{
    if (!s_window || !s_window->shown()) return;
    g_layout.video_x = s_window->x();
    g_layout.video_y = s_window->y();
    g_layout.video_w = s_window->w();
    g_layout.video_h = s_window->h();
}

void video_window_destroy()
{
    if (s_window) {
        video_window_capture_config();
        s_window->hide();
        delete s_window;
        s_window = nullptr;
    }
}

#else

void video_window_show(bool) {}
void video_window_suspend() {}
bool video_window_shown() { return false; }
void video_window_refresh(bool, bool) {}
void video_window_capture_config() {}
void video_window_destroy() {}

#endif

} // namespace zui
