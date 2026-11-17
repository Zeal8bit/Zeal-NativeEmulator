/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Watch panel (main debugger window, right column). Placeholder. */

#include <FL/fl_draw.H>
#include "panel.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

class WatchPanel : public Panel {
public:
    WatchPanel(int X, int Y, int W, int H) : Panel(X, Y, W, H, "WATCH")
    {
        end();
    }

    void refresh(bool paused, bool force) override
    {
        (void) paused;
        (void) force;
    }

    void draw() override
    {
        Panel::draw();
        fl_font(FL_HELVETICA, 11);
        fl_color(COLOR_DIM);
        fl_draw("Watch expressions: coming soon.", x() + 10, top() + 4, w() - 20, 20,
                FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
    }
};

Panel* create_watch_panel(int x, int y, int w, int h)
{
    return new WatchPanel(x, y, w, h);
}

#endif

} // namespace zui
