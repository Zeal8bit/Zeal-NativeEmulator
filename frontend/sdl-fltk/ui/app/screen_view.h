/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Emulator screen: scales the RGB565 frame on the CPU with nearest-neighbour
 * sampling into a letterboxed 4:3 rectangle, and turns host keyboard/pointer
 * events into emulated keyboard and SNES mouse input. */

#pragma once

#include <cstdint>
#include <vector>
#include <FL/Fl_Widget.H>
#include "common/machine.h"

namespace zui::app {

class ScreenView : public Fl_Widget {
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
    void rebuild_image(int dst_w, int dst_h);

    const uint16_t* m_pixels = nullptr;
    int m_frame_w = FZ_SCREEN_WIDTH;
    int m_frame_h = FZ_SCREEN_HEIGHT;
    bool m_dirty = true;
    /* Packed RGB888 copies: the frame at its native size and the picture at
     * the on-screen size, rebuilt only when the frame or the widget changes. */
    std::vector<uint8_t> m_native;
    std::vector<uint8_t> m_scaled;
    std::vector<int> m_xmap;
    int m_src_w = 0;
    int m_src_h = 0;
    int m_img_w = 0;
    int m_img_h = 0;
    int m_map_src_w = 0;
    bool m_locked = false;
    int m_last_x = -1;
    int m_last_y = -1;
    float m_zoom = 0.0f;
};

} // namespace zui::app
