/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Emulator screen: an OpenGL window that scales the 640x480 RGB565 frame
 * with nearest filtering into a letterboxed 4:3 rectangle, and that turns
 * host keyboard/pointer events into emulated keyboard and SNES mouse input. */

#include "app/screen_view.h"
#include <cstdint>
#include <cstring>
#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Window.H>
#include <FL/gl.h>
#include <FL/platform.H>
#include "app/keymap.h"
#include "app/main_window.h"
#include "app/notifications.h"
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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
/* Power-of-two texture holding the 640x480 frame (portable to old GL). */
#define TEX_W 1024
#define TEX_H 512

#if defined(FLTK_USE_X11) || (FL_API_VERSION < 10400 && !defined(_WIN32) && !defined(__APPLE__))
#define ZUI_X11_WARP 1
#endif

namespace zui::app {

using namespace zui::widgets;
using namespace zui::theme;

/* ------------------------------------------------------------------ */
/*  ScreenView                                                         */
/* ------------------------------------------------------------------ */

ScreenView::ScreenView(int x, int y, int w, int h)
    : Fl_Gl_Window(x, y, w, h)
{
    mode(FL_RGB | FL_DOUBLE);
    box(FL_NO_BOX);
}

ScreenView::~ScreenView()
{
    if (m_locked) lock_pointer(false);
}

void ScreenView::present(const uint16_t* pixels, int width, int height)
{
    m_pixels = pixels;
    m_frame_w = width > 0 && width <= FZ_SCREEN_WIDTH ? width : FZ_SCREEN_WIDTH;
    m_frame_h = height > 0 && height <= FZ_SCREEN_HEIGHT ? height : FZ_SCREEN_HEIGHT;
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

void ScreenView::reset_texture()
{
    m_texture = 0;
}

void ScreenView::draw()
{
    if (!context_valid()) reset_texture();
    if (!valid()) {
        glViewport(0, 0, pixel_w(), pixel_h());
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
    }

    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_texture == 0) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        m_texture = tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (m_pixels) {
        /* Expand RGB565 to RGBA8 on the CPU: 16-bit texture formats are
         * not dependable across GL drivers. */
        static uint32_t s_rgba[FZ_SCREEN_WIDTH * FZ_SCREEN_HEIGHT];
        uint8_t* dst = (uint8_t*) s_rgba;
        for (int y = 0; y < m_frame_h; y++) {
            const uint16_t* src = m_pixels + y * FZ_SCREEN_WIDTH;
            for (int x = 0; x < m_frame_w; x++) {
                const uint16_t p = src[x];
                const uint8_t r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
                dst[0] = (uint8_t) ((r << 3) | (r >> 2));
                dst[1] = (uint8_t) ((g << 2) | (g >> 4));
                dst[2] = (uint8_t) ((b << 3) | (b >> 2));
                dst[3] = 0xFF;
                dst += 4;
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frame_w, m_frame_h,
                        GL_RGBA, GL_UNSIGNED_BYTE, s_rgba);
    }

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w(), h(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    int px, py, pw, ph;
    picture_rect(px, py, pw, ph);
    const float u = (float) m_frame_w / TEX_W;
    const float v = (float) m_frame_h / TEX_H;
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(px, py);
    glTexCoord2f(u, 0); glVertex2i(px + pw, py);
    glTexCoord2f(u, v); glVertex2i(px + pw, py + ph);
    glTexCoord2f(0, v); glVertex2i(px, py + ph);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    /* Transient notification, top-right corner of the picture */
    const char* text = notif_current_text();
    if (text) {
        /* gl_draw() leaves texture/blend state behind that breaks the frame
         * texture on later draws (seen on macOS): isolate it completely. */
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        ortho();
        gl_font(FL_HELVETICA_BOLD, 18);
        const int tw = (int) gl_width(text);
        const int tx = px + pw - tw - 16;
        const int ty = h() - (py + 30);
        gl_color(fl_rgb_color(0, 0, 0));
        gl_draw(text, tx + 1, ty - 1);
        gl_color(fl_rgb_color(0xff, 0xe3, 0x45));
        gl_draw(text, tx, ty);
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopClientAttrib();
        glPopAttrib();
    }
}

void ScreenView::lock_pointer(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    if (window()) window()->cursor(locked ? FL_CURSOR_NONE : FL_CURSOR_DEFAULT);
    cursor(locked ? FL_CURSOR_NONE : FL_CURSOR_DEFAULT);
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
    if (m_last_x >= 0) fz_mouse_motion((float) (x - m_last_x), (float) (y - m_last_y));
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
    if (x > margin && y > margin && x < w() - margin && y < h() - margin) return;
#if defined(_WIN32) || defined(ZUI_X11_WARP)
    const int cx = w() / 2;
    const int cy = h() / 2;
#endif
#if defined(_WIN32)
    POINT p = { (LONG) (x_root() + cx), (LONG) (y_root() + cy) };
    SetCursorPos(p.x, p.y);
    m_last_x = cx;
    m_last_y = cy;
#elif defined(ZUI_X11_WARP)
#if FL_API_VERSION >= 10400
    Display* display = fl_x11_display();
#else
    Display* display = fl_display;
#endif
    if (display && shown()) {
        XWarpPointer(display, None, fl_xid(this), 0, 0, 0, 0, cx, cy);
        m_last_x = cx;
        m_last_y = cy;
    }
#endif
}

/** Physical key of the current event. With Num Lock off, FLTK turns keypad
 *  keys into arrows/Home/End; the original key keeps them keypad keys. */
namespace {

int host_key()
{
#if FL_API_VERSION >= 10303 || FL_MAJOR_VERSION > 1 || (FL_MAJOR_VERSION == 1 && FL_MINOR_VERSION > 3)
    const int original = Fl::event_original_key();
    if (original >= FL_KP && original <= FL_KP_Last) return original;
#endif
    return Fl::event_key();
}

} // namespace

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
            if (!m_locked) fz_mouse_hover(false);
            m_last_x = m_last_y = -1;
            return 1;
        case FL_MOVE:
        case FL_DRAG:
            pointer_motion();
            fz_mouse_buttons(Fl::event_button1() != 0, Fl::event_button3() != 0);
            return 1;
        case FL_PUSH:
            if (Fl::focus() != this) take_focus();
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
            if (down && !(fz_debugger_enabled() && config.debugger.keyboard_passthru)) {
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
            if (key != 0) fz_key_event(key, down);
            return 1;
        }
        default:
            break;
    }
    return Fl_Gl_Window::handle(event);
}

} // namespace zui::app
