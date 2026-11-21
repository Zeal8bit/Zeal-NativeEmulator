/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "hardware/hw_register_view.h"

namespace zui::hardware {

class SoundRegistersView : public HwRegisterView {
public:
    SoundRegistersView(int X, int Y, int W, int H);

protected:
    void build() override;
};

} // namespace zui::hardware
