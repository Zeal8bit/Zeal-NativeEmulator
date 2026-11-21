/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <FL/Fl_Widget.H>
#include "video/vram_image.h"

namespace zui::video {

class Inspector : public Fl_Widget {
public:
    Inspector(int X, int Y, int W, int H, VramImage* image);
    int handle(int event) override;
    void draw() override;

private:
    VramImage* m_image;
};

} // namespace zui::video
