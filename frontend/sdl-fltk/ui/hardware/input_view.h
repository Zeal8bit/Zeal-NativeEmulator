/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/input.h"
#include "widgets/panel.h"

namespace zui::hardware {

class InputView : public widgets::Panel {
public:
    InputView(int X, int Y, int W, int H);
    ~InputView() override;
    void refresh(bool paused, bool force) override;
    void draw() override;
    int handle(int event) override;

private:
    void update();
    static void cb_tick(void* data);

    /* -------------------------------------------------------------- */

    static int section(int X, int Y, int W, const char* title, const char* right = nullptr);

    /** Small rectangle chip; returns its width. */
    static int chip(int X, int Y, int H, const char* label, bool lit, Fl_Font font, int size);
    static bool held(const fz_keyboard_state_t& kb, uint16_t code);
    void draw_keyboard(int X, int Y, int W, int bottom);

    /* -------------------------------------------------------------- */

    /** SNES pad outline with lit buttons; returns its height. */
    static int draw_pad(int X, int Y, uint16_t pressed, bool available);
    void draw_snes(int X, int Y, int W, int bottom);
};

} // namespace zui::hardware
