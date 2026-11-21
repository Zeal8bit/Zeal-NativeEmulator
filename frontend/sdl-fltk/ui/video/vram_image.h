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

namespace zui::video {

class Inspector;

class VramImage : public Fl_Widget {
public:
    VramImage(int X, int Y, int W, int H);

    Inspector* inspector = nullptr;
    fz_image_t img = {};
    bool valid = false;
    int zoom = 2;
    dbg_vram_t view = DBG_TILEMAP_LAYER0;
    int hover_cell = -1;
    int pinned_cell = -1;

    void update(dbg_vram_t v, int z);
    int cell_count() const;
    void draw() override;
    int handle(int event) override;

private:
    int cell_at(int ex, int ey) const;

    std::vector<uint8_t> m_scaled;
};

} // namespace zui::video
