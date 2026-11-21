/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Emulator screen: an OpenGL window that scales the 640x480 RGB565 frame
 * with nearest filtering into a letterboxed 4:3 rectangle, and that turns
 * host keyboard/pointer events into emulated keyboard and SNES mouse input. */

#pragma once

#include <cstdint>
#include <FL/Fl_Gl_Window.H>
#include "common/machine.h"

namespace zui::app {

class ScreenView : public Fl_Gl_Window {
public:
    ScreenView(int x, int y, int w, int h);
    ~ScreenView() override;
    void present(const uint16_t* pixels, int width, int height);
    /** Rectangle (widget coordinates) in which the 4:3 picture is drawn. */
    void picture_rect(int& x, int& y, int& w, int& h) const;
    /** Picture scale inside the widget (1 = 640x480); 0 fits the widget.
     *  Never drawn larger than what fits. */
    void zoom(float scale);
    float zoom() const { return m_zoom; }
    /** Largest scale that fits the widget. */
    float fit_scale() const;
    void lock_pointer(bool locked);
    bool pointer_locked() const { return m_locked; }
    int handle(int event) override;
    void draw() override;

private:
    void pointer_motion();
    void reset_texture();

    const uint16_t* m_pixels = nullptr;
    int m_frame_w = FZ_SCREEN_WIDTH;
    int m_frame_h = FZ_SCREEN_HEIGHT;
    unsigned m_texture = 0;
    bool m_locked = false;
    int m_last_x = -1;
    int m_last_y = -1;
    float m_zoom = 0.0f;
};

} // namespace zui::app
