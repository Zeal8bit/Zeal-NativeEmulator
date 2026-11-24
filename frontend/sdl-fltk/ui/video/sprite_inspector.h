/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Widget.H>
#include "common/machine.h"
#include "video/sprite_data.h"

namespace zui::video {

class SpriteInspector : public Fl_Widget {
public:
    SpriteInspector(int X, int Y, int W, int H, SpriteData* data);

    Fl_Widget* sheet = nullptr;

    void draw() override;
    int handle(int event) override;

private:
    void map_rect(int& mx, int& my, int& mw, int& mh) const;
    void sprite_rect(int index, int& rx, int& ry, int& rw, int& rh) const;

    SpriteData* m_data;
    std::vector<uint8_t> m_buf;
    int m_mx = 0, m_my = 0, m_mw = 0, m_mh = 0;
};

} // namespace zui::video
