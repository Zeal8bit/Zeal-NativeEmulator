/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Base of every debugger panel and tool view: a surface with a retro
 * header strip (title, optional right-aligned text). */

#pragma once

#include <FL/Fl_Group.H>

namespace zui::widgets {

class Panel : public Fl_Group {
public:
    Panel(int X, int Y, int W, int H, const char* title);

    /** Right-aligned text in the header (state, counts...). */
    void extra(const char* text);
    void draw() override;

    /** First y below the header. */
    int top() const;
    const char* extra_text() const { return m_extra; }
    /**
     * Update the content. `paused`: the machine is stopped in the debugger.
     * `force`: a pause/step/user action happened (always update); otherwise
     * this is a periodic refresh while running (~10 Hz in the main window,
     * ~4 Hz in tool windows).
     */
    virtual void refresh(bool paused, bool force) = 0;

private:
    const char* m_title;
    char m_extra[64] = "";
};

} // namespace zui::widgets
