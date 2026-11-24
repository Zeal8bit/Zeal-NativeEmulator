/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Emulator screen: scales the 640x480 RGB565 frame on the CPU with
 * nearest-neighbour sampling into a letterboxed 4:3 rectangle, and turns host
 * keyboard/pointer events into emulated keyboard and SNES mouse input. */

#include "app/screen_view.h"
#include <cstdint>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>
#include "app/keymap.h"
#include "app/main_window.h"
#include "app/notifications.h"
#include "common/frame_convert.h"
#include "common/machine.h"
#include "theme/theme.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "hw/keys.h"
}

#if defined(FLTK_USE_X11) || (FL_API_VERSION < 10400 && !defined(_WIN32) && !defined(__APPLE__))
#define ZUI_X11_WARP 1
#endif

namespace zui::app {

using namespace zui::theme;

/* ------------------------------------------------------------------ */
/*  ScreenView                                                         */
/* ------------------------------------------------------------------ */

ScreenView::ScreenView(int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h)
{
    box(FL_NO_BOX);
}

ScreenView::~ScreenView()
{
    if (m_locked) {
        lock_pointer(false);
    }
}

void ScreenView::present(const uint16_t* pixels, int width, int height)
{
    m_pixels = pixels;
    m_frame_w = width > 0 && width <= FZ_SCREEN_WIDTH ? width : FZ_SCREEN_WIDTH;
    m_frame_h = height > 0 && height <= FZ_SCREEN_HEIGHT ? height : FZ_SCREEN_HEIGHT;
    m_dirty = true;
    redraw();
}

void ScreenView::picture_rect(int& px, int& py, int& pw, int& ph) const
{
    const float ratio = (float) FZ_SCREEN_WIDTH / FZ_SCREEN_HEIGHT;
    if ((float) w() / (h() > 0 ? h() : 1) > ratio) {
        ph = h();
        pw = (int) (h() * ratio + 0.5f);
    } else {
        pw = w();
        ph = (int) (w() / ratio + 0.5f);
    }
    /* A zoom smaller than the fit shrinks the picture, centred */
    if (m_zoom > 0.0f) {
        const int zw = (int) (FZ_SCREEN_WIDTH * m_zoom + 0.5f);
        if (zw < pw) {
            pw = zw;
            ph = (int) (FZ_SCREEN_HEIGHT * m_zoom + 0.5f);
        }
    }
    px = (w() - pw) / 2;
    py = (h() - ph) / 2;
}

void ScreenView::zoom(float scale)
{
    m_zoom = scale > 0.0f ? scale : 0.0f;
    redraw();
}

float ScreenView::fit_scale() const
{
    const float sx = (float) w() / FZ_SCREEN_WIDTH;
    const float sy = (float) h() / FZ_SCREEN_HEIGHT;
    return sx < sy ? sx : sy;
}

void ScreenView::rebuild_image(int dst_w, int dst_h)
{
    const std::size_t native_size = (std::size_t) m_frame_w * m_frame_h * 3;
    if (m_native.size() != native_size || m_src_w != m_frame_w || m_src_h != m_frame_h) {
        m_native.resize(native_size);
        m_src_w = m_frame_w;
        m_src_h = m_frame_h;
    }
    common::frame_to_rgb888(m_pixels, m_frame_w, m_frame_h, FZ_SCREEN_WIDTH, m_native.data());

    const std::size_t scaled_size = (std::size_t) dst_w * dst_h * 3;
    if (m_scaled.size() != scaled_size) {
        m_scaled.resize(scaled_size);
    }
    m_img_w = dst_w;
    m_img_h = dst_h;
    if ((int) m_xmap.size() != dst_w || m_map_src_w != m_frame_w) {
        m_xmap.resize(dst_w);
        for (int x = 0; x < dst_w; x++) {
            m_xmap[x] = (int) ((long) x * m_frame_w / dst_w);
        }
        m_map_src_w = m_frame_w;
    }
    common::rgb888_scale(m_native.data(), m_frame_w, m_frame_h,
                         m_scaled.data(), dst_w, dst_h, m_xmap.data());
}

void ScreenView::draw()
{
    int px, py, pw, ph;
    picture_rect(px, py, pw, ph);
    const bool resized = pw != m_img_w || ph != m_img_h;
    if (m_pixels != nullptr && pw > 0 && ph > 0 && (m_dirty || resized)) {
        rebuild_image(pw, ph);
        m_dirty = false;
    }

    /* Fill only the letterbox bars; the picture covers the rest. */
    fl_color(COLOR_BG);
    if (py > 0) {
        fl_rectf(x(), y(), w(), py);
    }
    if (py + ph < h()) {
        fl_rectf(x(), y() + py + ph, w(), h() - (py + ph));
    }
    if (px > 0) {
        fl_rectf(x(), y() + py, px, ph);
    }
    if (px + pw < w()) {
        fl_rectf(x() + px + pw, y() + py, w() - (px + pw), ph);
    }
    if (!m_scaled.empty() && m_img_w == pw && m_img_h == ph) {
        fl_draw_image((const uchar*) m_scaled.data(), x() + px, y() + py, pw, ph, 3);
    }

    /* Transient notification, bottom-right corner of the picture */
    const char* text = notif_current_text();
    if (text != nullptr) {
        fl_font(FL_HELVETICA_BOLD, 18);
        const int tw = (int) fl_width(text);
        const int tx = x() + px + pw - tw - 16;
        const int ty = y() + h() - (py + 30);
        fl_color(fl_rgb_color(0, 0, 0));
        fl_draw(text, tx + 1, ty - 1);
        fl_color(fl_rgb_color(0xff, 0xe3, 0x45));
        fl_draw(text, tx, ty);
    }
}

void ScreenView::lock_pointer(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    if (window()) {
        window()->cursor(locked ? FL_CURSOR_NONE : FL_CURSOR_DEFAULT);
    }
#ifdef __APPLE__
    CGAssociateMouseAndMouseCursorPosition(locked ? false : true);
    int32_t dx, dy;
    CGGetLastMouseDelta(&dx, &dy); /* discard the delta that triggered capture */
#endif
    m_last_x = m_last_y = -1;
    if (!locked) {
        fz_mouse_hover(Fl::belowmouse() == this);
    }
}

void ScreenView::pointer_motion()
{
    const int x = Fl::event_x();
    const int y = Fl::event_y();

#ifdef __APPLE__
    if (m_locked) {
        /* The cursor is frozen; only raw deltas move the SNES mouse */
        int32_t dx, dy;
        CGGetLastMouseDelta(&dx, &dy);
        fz_mouse_motion((float) dx, (float) dy);
        return;
    }
#endif
    if (m_last_x >= 0) {
        fz_mouse_motion((float) (x - m_last_x), (float) (y - m_last_y));
    }
    m_last_x = x;
    m_last_y = y;

    if (!m_locked) {
        int px, py, pw, ph;
        picture_rect(px, py, pw, ph);
        fz_mouse_hover(x >= px && y >= py && x < px + pw && y < py + ph);
        return;
    }

    /* Keep a captured pointer away from the edges by recentring it */
    const int margin = 32;
    if (x > margin && y > margin && x < w() - margin && y < h() - margin) {
        return;
    }
#if defined(_WIN32) || defined(ZUI_X11_WARP)
    const int cx = w() / 2;
    const int cy = h() / 2;
    if (window() == nullptr) {
        return;
    }
#endif
#if defined(_WIN32)
    POINT p = { (LONG) (window()->x_root() + cx), (LONG) (window()->y_root() + cy) };
    SetCursorPos(p.x, p.y);
    m_last_x = cx;
    m_last_y = cy;
#elif defined(ZUI_X11_WARP)
#if FL_API_VERSION >= 10400
    Display* display = fl_x11_display();
#else
    Display* display = fl_display;
#endif
    if (display && window() != nullptr && window()->shown()) {
        XWarpPointer(display, None, fl_xid(window()), 0, 0, 0, 0, cx, cy);
        m_last_x = cx;
        m_last_y = cy;
    }
#endif
}

/** Physical key of the current event. With Num Lock off, FLTK turns keypad
 *  keys into arrows/Home/End; the original key keeps them keypad keys. */

static int host_key()
{
#if FL_API_VERSION >= 10303 || FL_MAJOR_VERSION > 1 || (FL_MAJOR_VERSION == 1 && FL_MINOR_VERSION > 3)
    const int original = Fl::event_original_key();
    if (original >= FL_KP && original <= FL_KP_Last) {
        return original;
    }
#endif
    return Fl::event_key();
}

int ScreenView::handle(int event)
{
    switch (event) {
        case FL_FOCUS:
            return 1;
        case FL_UNFOCUS:
            /* Keyboard focus went to another widget, dialog or application:
             * nothing stays held, and a captured pointer is given back */
            fz_keys_release_all();
            fz_mouse_release_all();
            return 1;
        case FL_ENTER:
            m_last_x = Fl::event_x();
            m_last_y = Fl::event_y();
            pointer_motion();
            return 1;
        case FL_LEAVE:
            if (!m_locked) {
                fz_mouse_hover(false);
            }
            m_last_x = m_last_y = -1;
            return 1;
        case FL_MOVE:
        case FL_DRAG:
            pointer_motion();
            fz_mouse_buttons(Fl::event_button1() != 0, Fl::event_button3() != 0);
            return 1;
        case FL_PUSH:
            if (Fl::focus() != this) {
                take_focus();
            }
            if (Fl::event_button() == FL_MIDDLE_MOUSE) {
                fz_mouse_capture_toggle();
            }
            fz_mouse_buttons(Fl::event_button1() != 0, Fl::event_button3() != 0);
            return 1;
        case FL_RELEASE:
            fz_mouse_buttons(Fl::event_button1() != 0, Fl::event_button3() != 0);
            return 1;
        case FL_MOUSEWHEEL:
            fz_mouse_wheel((float) -Fl::event_dy());
            return 1;
        case FL_KEYDOWN:
        case FL_KEYUP: {
            const bool down = event == FL_KEYDOWN;
            if (down && !config.debugger.keyboard_passthru) {
                MainWindow* mw = main_window();
                /* Let menu shortcuts through; everything else is the machine's */
                if (mw && mw->menubar() && mw->menubar()->menu() &&
                    mw->menubar()->menu()->test_shortcut()) {
                    return 0;
                }
            }
            const int key = fltk_to_zeal_key(host_key());
#ifdef __APPLE__
            /* macOS reports Caps Lock as its toggle state (down when it turns
             * on, up when it turns off): every event is one physical press. */
            if (key == ZEAL_KEY_CAPS_LOCK) {
                fz_key_event(key, true);
                fz_key_event(key, false);
                return 1;
            }
            /* macOS sends no key-up for keys released while Cmd is held */
            if (!down && (key == ZEAL_KEY_LEFT_SUPER || key == ZEAL_KEY_RIGHT_SUPER)) {
                fz_keys_release_nonmodifiers();
            }
#endif
            if (key != 0) {
                fz_key_event(key, down);
            }
            return 1;
        }
        default:
            break;
    }
    return Fl_Widget::handle(event);
}

} // namespace zui::app
