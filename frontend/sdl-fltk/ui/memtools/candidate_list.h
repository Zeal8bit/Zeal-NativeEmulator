/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Widget.H>
#include "memtools/snapshot_source.h"

namespace zui::memtools {

class CandidateList : public Fl_Widget {
public:
    CandidateList(int X, int Y, int W, int H, Fl_Scrollbar* bar);

    const std::vector<uint16_t>* items = nullptr;
    const SnapshotSource* source = nullptr;

    int rows() const;
    int count() const;
    void scroll_to(int top);
    void sync();
    void draw() override;
    int handle(int event) override;
    void resize(int X, int Y, int W, int H) override;

private:
    int index_at(int ey) const;
    void clamp();

    Fl_Scrollbar* m_bar;
    int m_top = 0;
    int m_hover = -1;
};

} // namespace zui::memtools
