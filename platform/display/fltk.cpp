// SPDX-License-Identifier: Apache-2.0
/**
 * @file fltk.cpp
 * @brief FLTK implementation of the platform seam.
 *
 * The desktop build links this one; WebAssembly links platform/display/raylib.c. It owns
 * the emulator window and turns FLTK events into the seam's key and pointer state, so
 * the emulator core never sees a windowing library.
 *
 * Audio and controllers have their own seams: see include/platform/audio.h and
 * include/platform/controller.h.
 */
#include "platform/input.h"
#include "platform/display.h"
#include "ui/fltk/input.h"
#include "utils/paths.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
/* ------------------------------------------------------------------ */
/*  State                                                              */
/* ------------------------------------------------------------------ */

bool g_should_close = false;
bool g_shown = false;
int g_target_fps = 0;
double g_start_time = 0;
display_color_t g_background{0, 0, 0, 255};

/* Keys currently held, indexed by seam code. */
bool g_key_down[INPUT_KEY_COUNT] = {};
/* Keys pressed since the last poll, in press order. */
std::vector<int> g_key_queue;

bool g_mouse_down[INPUT_MOUSE_BUTTON_COUNT] = {};
input_point_t g_mouse_position{0, 0};
int g_mouse_dx = 0, g_mouse_dy = 0;
int g_mouse_wheel = 0;
bool g_mouse_captured = false;

double now_seconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/* ------------------------------------------------------------------ */
/*  Screen widget                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Draws the frame the emulator hands over each refresh.
 *
 * The blitter produces RGB565 in memory, so the widget converts it to the RGB888 that
 * fl_draw_image wants and lets FLTK scale it into the destination rectangle.
 */
class ScreenWidget : public Fl_Widget
{
  public:
    const void *frame = nullptr;
    int frame_width = 0;
    int frame_height = 0;
    int frame_pitch = 0;
    bool frame_rgb565 = true;
    display_rect_t dest{0, 0, 0, 0};

    ScreenWidget(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_color(fl_rgb_color(g_background.r, g_background.g, g_background.b));
        fl_rectf(x(), y(), w(), h());
        if (frame == nullptr || frame_width <= 0 || frame_height <= 0)
            return;
        const size_t count = (size_t)frame_width * frame_height;
        rgb.resize(count * 3);
        if (frame_rgb565) {
            const uint16_t *src = static_cast<const uint16_t *>(frame);
            for (size_t i = 0; i < count; i++) {
                const uint16_t p = src[i];
                rgb[i * 3 + 0] = (unsigned char)(((p >> 11) & 0x1F) * 255 / 31);
                rgb[i * 3 + 1] = (unsigned char)(((p >> 5) & 0x3F) * 255 / 63);
                rgb[i * 3 + 2] = (unsigned char)((p & 0x1F) * 255 / 31);
            }
        } else {
            const unsigned char *src = static_cast<const unsigned char *>(frame);
            for (size_t i = 0; i < count; i++) {
                rgb[i * 3 + 0] = src[i * 4 + 0];
                rgb[i * 3 + 1] = src[i * 4 + 1];
                rgb[i * 3 + 2] = src[i * 4 + 2];
            }
        }
        Fl_RGB_Image image(rgb.data(), frame_width, frame_height, 3);
        image.draw((int)dest.x, (int)dest.y, (int)dest.width, (int)dest.height);
    }

  private:
    std::vector<unsigned char> rgb;
};

/* ------------------------------------------------------------------ */
/*  Window                                                             */
/* ------------------------------------------------------------------ */

class HostWindow : public Fl_Double_Window
{
  public:
    HostWindow(int w, int h, const char *title) : Fl_Double_Window(w, h, title) {}

    int handle(int event) override
    {
        switch (event) {
        case FL_KEYDOWN:
        case FL_KEYUP: {
            const unsigned key = fltk_key_to_display(Fl::event_key());
            if (key == INPUT_KEY_NONE)
                break;
            if (event == FL_KEYDOWN) {
                /* FLTK repeats FL_KEYDOWN while a key is held; the emulator does its own
                 * repeat timing, so only the first press is queued. */
                if (!g_key_down[key])
                    g_key_queue.push_back((int)key);
                g_key_down[key] = true;
            } else {
                g_key_down[key] = false;
            }
            return 1;
        }
        case FL_PUSH:
        case FL_RELEASE: {
            const int button = Fl::event_button();
            if (button < 1 || button > INPUT_MOUSE_BUTTON_COUNT)
                break;
            g_mouse_down[button - 1] = (event == FL_PUSH);
            g_mouse_position = {Fl::event_x(), Fl::event_y()};
            return 1;
        }
        case FL_MOVE:
        case FL_DRAG: {
            const input_point_t next{Fl::event_x(), Fl::event_y()};
            g_mouse_dx += next.x - g_mouse_position.x;
            g_mouse_dy += next.y - g_mouse_position.y;
            g_mouse_position = next;
            return 1;
        }
        case FL_MOUSEWHEEL:
            /* FLTK documents event_dy() as down-positive; the seam is up-positive. */
            g_mouse_wheel -= Fl::event_dy();
            return 1;
        case FL_ENTER:
        case FL_LEAVE:
        case FL_FOCUS:
        case FL_UNFOCUS:
            return 1;
        default:
            break;
        }
        return Fl_Double_Window::handle(event);
    }
};

HostWindow *g_window = nullptr;
ScreenWidget *g_screen = nullptr;

void on_window_close(Fl_Widget *, void *) { g_should_close = true; }

} // namespace

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

bool display_init(const display_config_t *config)
{
    if (config == nullptr)
        return false;
    g_start_time = now_seconds();
    g_window = new HostWindow(config->width, config->height, config->title);
    if (config->resizable) {
        g_window->resizable(g_window);
        g_window->size_range(320, 240);
    }
    g_window->begin();
    g_screen = new ScreenWidget(0, 0, config->width, config->height);
    g_window->end();
    g_window->callback(on_window_close);
    g_window->show();
    g_shown = true;
    g_should_close = false;
    Fl::check();
    return true;
}

void display_shutdown(void)
{
    delete g_window;
    g_window = nullptr;
    g_screen = nullptr;
}

bool display_should_close(void) { return g_should_close; }

/* ------------------------------------------------------------------ */
/*  Main loop                                                          */
/* ------------------------------------------------------------------ */

/* Set while display_run() owns the loop. */
bool (*g_tick)(void *) = nullptr;
void *g_tick_user = nullptr;

/* Frame interval the emulator asked for, defaulting to 60 while it has not said. */
double frame_interval()
{
    return 1.0 / (g_target_fps > 0 ? g_target_fps : 60.0);
}

void run_tick(void *)
{
    if (g_should_close || g_tick == nullptr || !g_tick(g_tick_user)) {
        /* Fl::run() returns once there is no window left to watch, so every window has
         * to go: the debugger shell is not the one this backend created. */
        Fl::hide_all_windows();
        return;
    }
    /* Re-arming from the callback paces the loop; FLTK processes events as it waits
     * for this timeout. */
    Fl::repeat_timeout(frame_interval(), run_tick);
}

void display_run(bool (*tick)(void *user), void *user)
{
    g_tick = tick;
    g_tick_user = user;
    Fl::add_timeout(frame_interval(), run_tick);
    Fl::run();
    Fl::remove_timeout(run_tick);
    g_tick = nullptr;
    g_tick_user = nullptr;
}

void display_poll(void)
{
    Fl::check();
}

/* ------------------------------------------------------------------ */
/*  Window                                                             */
/* ------------------------------------------------------------------ */

int display_width(void) { return g_window ? g_window->w() : 0; }
int display_height(void) { return g_window ? g_window->h() : 0; }

void display_resize(int width, int height)
{
    if (g_window)
        g_window->size(width, height);
}

void display_show(bool visible)
{
    if (!g_window)
        return;
    if (visible)
        g_window->show();
    else
        g_window->hide();
    g_shown = visible;
}

void display_focus(void)
{
    if (g_window)
        g_window->take_focus();
}

bool display_focused(void) { return g_window && g_window->shown() && Fl::focus() != nullptr; }

void display_frame_rate(int fps)
{
    g_target_fps = fps;
}

/* ------------------------------------------------------------------ */
/*  Frame                                                              */
/* ------------------------------------------------------------------ */

void display_frame_begin(void)
{
    if (g_screen)
        g_screen->frame = nullptr;
}

void display_frame_end(void)
{
    if (g_screen)
        g_screen->redraw();
    Fl::flush();
}

void display_clear(display_color_t color) { g_background = color; }

void display_present(const void *pixels, int width, int height, int pitch,
                       display_format_t format, display_rect_t dest)
{
    if (g_screen == nullptr)
        return;
    g_screen->frame = pixels;
    g_screen->frame_width = width;
    g_screen->frame_height = height;
    g_screen->frame_pitch = pitch;
    g_screen->frame_rgb565 = (format == DISPLAY_RGB565);
    g_screen->dest = dest;
    /* Nothing repaints until the frame ends, so the buffer stays valid while drawn. */
}

void display_text(int x, int y, const char *text, int size, display_color_t color)
{
    if (text == nullptr || g_window == nullptr)
        return;
    fl_color(fl_rgb_color(color.r, color.g, color.b));
    fl_font(FL_HELVETICA, size);
    fl_push_clip(0, 0, g_window->w(), g_window->h());
    fl_draw(text, x, y + size);
    fl_pop_clip();
}

int display_text_width(const char *text, int size)
{
    if (text == nullptr)
        return 0;
    fl_font(FL_HELVETICA, size);
    return (int)fl_width(text);
}

void display_fps(int x, int y)
{
    static double last = 0;
    static int frames = 0;
    static double shown = 0;
    const double now = display_time();
    frames++;
    if (now - last >= 0.5) {
        shown = frames / (now - last);
        frames = 0;
        last = now;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%d FPS", (int)(shown + 0.5));
    display_text(x, y, text, 20, (display_color_t){0, 228, 48, 255});
}

/* ------------------------------------------------------------------ */
/*  Font atlas                                                         */
/* ------------------------------------------------------------------ */

bool display_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16])
{
    char path[PATH_MAX];
    get_install_dir_file(path, "assets/fonts/BigBlue_Terminal_437TT.TTF");
    if (!path_exists(path)) {
        std::snprintf(path, sizeof(path), "assets/fonts/BigBlue_Terminal_437TT.TTF");
    }
#ifdef ZEAL_ASSETS_DIR
    if (!path_exists(path)) {
        std::snprintf(path, sizeof(path), "%s/fonts/BigBlue_Terminal_437TT.TTF", ZEAL_ASSETS_DIR);
    }
#endif
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
        return false;
    FT_Face face;
    if (FT_New_Face(library, path, 0, &face) != 0) {
        FT_Done_FreeType(library);
        return false;
    }
    FT_Set_Pixel_Sizes(face, 0, 16);
    /* Glyph bitmaps are positioned relative to the baseline, so anchor that to the
     * ascender of the 16px face and the cells line up. */
    const int baseline = (face->size->metrics.ascender + 63) >> 6;
    std::memset(alpha, 0, 256 * 8 * 16);
    for (unsigned i = 0; i < 256; i++) {
        if (FT_Load_Char(face, codepoints[i], FT_LOAD_RENDER) != 0)
            continue;
        const FT_Bitmap *bitmap = &face->glyph->bitmap;
        const int left = face->glyph->bitmap_left;
        const int top = face->glyph->bitmap_top;
        for (unsigned y = 0; y < bitmap->rows; y++) {
            for (unsigned x = 0; x < bitmap->width; x++) {
                const int dx = left + (int)x;
                const int dy = baseline - top + (int)y;
                if (dx < 0 || dx >= 8 || dy < 0 || dy >= 16)
                    continue;
                alpha[i * 128 + dy * 8 + dx] = bitmap->buffer[y * bitmap->pitch + x];
            }
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Time                                                               */
/* ------------------------------------------------------------------ */

double display_time(void) { return now_seconds() - g_start_time; }

void display_wait(double seconds)
{
    if (seconds > 0)
        Fl::wait(seconds);
}

/* ------------------------------------------------------------------ */
/*  Input                                                              */
/* ------------------------------------------------------------------ */

int input_key_pressed(void)
{
    if (g_key_queue.empty())
        return INPUT_KEY_NONE;
    const int key = g_key_queue.front();
    g_key_queue.erase(g_key_queue.begin());
    return key;
}

bool input_key_down(int key)
{
    return key > 0 && key < INPUT_KEY_COUNT && g_key_down[key];
}

bool input_key_up(int key) { return !input_key_down(key); }

bool input_mouse_down(int button)
{
    return button >= 0 && button < INPUT_MOUSE_BUTTON_COUNT && g_mouse_down[button];
}

input_point_t input_mouse_position(void) { return g_mouse_position; }

input_delta_t input_mouse_delta(void)
{
    const input_delta_t delta{g_mouse_dx, g_mouse_dy};
    /* Reset per frame, matching a polled delta rather than a consuming read. */
    g_mouse_dx = g_mouse_dy = 0;
    return delta;
}

int input_mouse_wheel(void)
{
    const int wheel = g_mouse_wheel;
    g_mouse_wheel = 0;
    return wheel;
}

void input_mouse_capture(bool capture)
{
    if (capture == g_mouse_captured)
        return;
    g_mouse_captured = capture;
    fl_cursor(capture ? FL_CURSOR_NONE : FL_CURSOR_DEFAULT);
}
