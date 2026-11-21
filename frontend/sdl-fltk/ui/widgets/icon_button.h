/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Flat toolbar button with a vector icon and optional text. */

#pragma once

#include <FL/Fl_Button.H>
#include "theme/theme.h"

namespace zui::widgets {

class IconButton : public Fl_Button {
public:
    IconButton(int x, int y, int w, int h, theme::Icon icon, const char* text = nullptr);
    void icon(theme::Icon icon) { m_icon = icon; redraw(); }
    void text(const char* text) { m_text = text; redraw(); }
    void icon_color(Fl_Color c) { m_color = c; redraw(); }
    void draw() override;
    int handle(int event) override;

private:
    theme::Icon m_icon;
    const char* m_text;
    Fl_Color m_color = 0;
};

} // namespace zui::widgets
