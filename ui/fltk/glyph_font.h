// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "debugger/frontend.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <array>
#include <vector>
class GlyphFont
{
    std::array<uint8_t, 256 * 8 * 16> atlas{};
    bool loaded = false;

  public:
    void load(const dbg_ui_init_args_t &host, const uint16_t *unicode)
    {
        uint32_t codes[256];
        for (unsigned i = 0; i < 256; i++)
            codes[i] = unicode[i];
        loaded = host.font_atlas && host.font_atlas(codes, atlas.data());
    }
    bool draw(uint8_t character, int x, int y, int width, int height, Fl_Color foreground,
              Fl_Color background) const
    {
        if (!loaded)
            return false;
        unsigned char fr, fg, fb, br, bg, bb;
        Fl::get_color(foreground, fr, fg, fb);
        Fl::get_color(background, br, bg, bb);
        std::vector<unsigned char> pixels(width * height * 3);
        for (int yy = 0; yy < height; yy++)
            for (int xx = 0; xx < width; xx++) {
                auto a = atlas[character * 128 + (yy * 16 / height) * 8 + xx * 8 / width];
                auto p = &pixels[(yy * width + xx) * 3];
                p[0] = (fr * a + br * (255 - a)) / 255;
                p[1] = (fg * a + bg * (255 - a)) / 255;
                p[2] = (fb * a + bb * (255 - a)) / 255;
            }
        fl_draw_image(pixels.data(), x, y, width, height, 3);
        return true;
    }
};
