/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "memtools/candidate_list.h"
#include "memtools/change_search.h"
#include "memtools/snapshot_source.h"

namespace zui::memtools {

class ChangeSearchWindow : public Fl_Double_Window {
public:
    static constexpr int W = 380;
    static constexpr int H = 460;

    ChangeSearchWindow();
    void source(const SnapshotSource& src) { m_source = src; }
    void paused();
    void draw() override;
    void resize(int X, int Y, int W_, int H_) override;

private:
    void apply();
    void update();
    void update_filter();
    void status(const char* text, Fl_Color c);
    static void cb_filter(Fl_Widget*, void* d);
    static void cb_apply(Fl_Widget*, void* d);
    static void cb_bar(Fl_Widget*, void* d);
    static void cb_reset(Fl_Widget*, void* d);

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

/** Open (or raise) the changed-bytes search window. */
void change_search_show(const SnapshotSource& source);
/** Called after the memory view took a new pause snapshot. */
void change_search_paused();
void change_search_destroy();

} // namespace zui::memtools
