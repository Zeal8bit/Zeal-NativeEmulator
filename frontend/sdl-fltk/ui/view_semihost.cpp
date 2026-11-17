/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Semihost performance counters (Code & Profiler window). */

#include <cstdio>
#include <FL/fl_draw.H>
#include "tool_window.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

class SemihostView : public Panel {
public:
    SemihostView(int X, int Y, int W, int H) : Panel(X, Y, W, H, "SEMIHOST COUNTERS")
    {
        tooltip("Click a counter to toggle break-on-update");
        extra("* running   ! break on update");
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
        redraw();
    }

    void draw() override
    {
        fl_color(COLOR_SURFACE);
        fl_rectf(x(), y(), w(), h());
        draw_header(x(), y(), w(), "SEMIHOST COUNTERS", extra_text());
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
        return Panel::handle(event);
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

static Panel* make_semihost(int x, int y, int w, int h)
{
    return new SemihostView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_CODE, 90, "Semihost", make_semihost);

#endif

} // namespace zui
