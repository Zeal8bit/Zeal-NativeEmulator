/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Widget.H>
#include "video/sprite_data.h"

namespace zui::video {

class SpriteSheet : public Fl_Widget {
public:
    static constexpr int COLUMNS = 8;

    SpriteSheet(int X, int Y, int W, int H, SpriteData* data);

    Fl_Widget* inspector = nullptr;
    int zoom = 2;

    int card_w() const;
    int card_h() const;
    void relayout();
    void draw() override;
    int handle(int event) override;

private:
    int sprite_at(int ex, int ey) const;

    SpriteData* m_data;
    std::vector<uint8_t> m_buf;
};

} // namespace zui::video
