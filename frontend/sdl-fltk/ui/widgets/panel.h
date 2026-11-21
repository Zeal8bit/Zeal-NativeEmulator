/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Base of every debugger panel and tool view: a surface with a retro
 * header strip (title, optional right-aligned text). In a PanelStack the
 * header also carries a collapse chevron and a maximise button; a panel may
 * add one on/off chip to its header (e.g. the CPU panel's MORE). */

#pragma once

#include <functional>
#include <vector>
#include <FL/Fl_Group.H>

namespace zui::widgets {

class Panel : public Fl_Group {
public:
    Panel(int X, int Y, int W, int H, const char* title);

    /** Right-aligned text in the header (state, counts...). */
    void extra(const char* text);
    void draw() override;
    int handle(int event) override;

    /** First y below the header. */
    int top() const;
    /** Height of the header strip (0 when hidden, e.g. inside tabs). */
    int header_h() const;
    void show_header(bool show);
    const char* title() const { return m_title; }
    const char* extra_text() const { return m_extra; }
    /**
     * Update the content. `paused`: the machine is stopped in the debugger.
     * `force`: a pause/step/user action happened (always update); otherwise
     * this is a periodic refresh while running (~10 Hz in the main window,
     * ~4 Hz in tool windows).
     */
    virtual void refresh(bool paused, bool force) = 0;
    /** Fixed height wanted in a PanelStack (0: shares the free space). */
    virtual int preferred_height() const { return 0; }

    /* Collapsing (title only). The owner lays panels out again when told. */
    void collapsible(bool on);
    bool collapsed() const { return m_collapsed; }
    virtual void set_collapsed(bool collapsed);
    /** Shown as a title only by its PanelStack (another panel is maximised);
     *  the collapsed state is kept. */
    void set_stack_hidden(bool hidden);
    /** Only the title is shown (collapsed or hidden by the stack). */
    bool content_hidden() const { return m_content_hidden; }
    /** The user clicked the title (collapse) or the maximise button. */
    std::function<void(Panel*)> on_collapse;
    std::function<void(Panel*)> on_maximize;
    /** Draw the maximise button as "restore" (the panel is maximised). */
    void maximized(bool on);

    /** One on/off chip in the header, left of the extra text. */
    void header_toggle(const char* label, bool on, std::function<void(bool)> changed);

protected:
    /** Header strip with chevron, title, chip, extra text and buttons. */
    void draw_panel_header();
    /** Header area to the right of the title, left of chips and buttons. */
    virtual void draw_header_content(int x, int y, int w);

private:
    int title_x() const;
    void chip_rect(int& cx, int& cw) const;
    int maximize_x() const;
    /** Hide the children (remembering which were visible) or show them again. */
    void sync_content();

    const char* m_title;
    char m_extra[64] = "";
    bool m_header = true;
    bool m_collapsible = false;
    bool m_collapsed = false;
    bool m_maximized = false;
    bool m_stack_hidden = false;
    bool m_content_hidden = false;
    std::vector<Fl_Widget*> m_was_visible; /* children to show again */
    const char* m_chip = nullptr;
    bool m_chip_on = false;
    std::function<void(bool)> m_chip_changed;
};

} // namespace zui::widgets
