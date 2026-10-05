// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>

// 12x12 monochrome toolbar icons. Each is expanded once into an RGBA image whose
// colour comes from the theme, so one bitmap serves every theme and no image
// library (libfltk_images) is needed. Translated from the following art:

namespace zeal_icons {
static const int ICON_SIZE = 12;
enum Icon { ICON_RUN, ICON_PAUSE, ICON_STEP, ICON_STEP_OVER, ICON_RESET, ICON_BREAKPOINT,
            ICON_SCREENSHOT, ICON_COUNT };
static const char *const run[ICON_SIZE] = {
    "............",
    "..#.........",
    "..##........",
    "..###.......",
    "..####......",
    "..#####.....",
    "..######....",
    "..#####.....",
    "..####......",
    "..###.......",
    "..##........",
    "..#.........",
};
static const char *const pause[ICON_SIZE] = {
    "............",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "...##..##...",
    "............",
};
static const char *const step[ICON_SIZE] = {
    "............",
    ".....##.....",
    ".....##.....",
    ".....##.....",
    ".....##.....",
    "..#######...",
    "...#####....",
    "....###.....",
    ".....#......",
    "............",
    "..#######...",
    "............",
};
static const char *const step_over[ICON_SIZE] = {
    "............",
    "........##..",
    ".......###..",
    "......####..",
    ".....##.....",
    "....##......",
    "...#........",
    "..#.........",
    ".#..........",
    ".##########.",
    "............",
    "............",
};
static const char *const reset[ICON_SIZE] = {
    "............",
    "....####....",
    "...#...##...",
    "..#.....##..",
    "..#......##.",
    ".#..........",
    ".#..........",
    ".#..........",
    "..#.........",
    "..#.........",
    "...######...",
    "............",
};
static const char *const breakpoint[ICON_SIZE] = {
    "............",
    "....####....",
    "..########..",
    ".##########.",
    ".##########.",
    ".##########.",
    ".##########.",
    ".##########.",
    ".##########.",
    "..########..",
    "....####....",
    "............",
};
static const char *const screenshot[ICON_SIZE] = {
    "............",
    ".....##.....",
    "..########..",
    ".#........#.",
    ".#..####..#.",
    ".#.#....#.#.",
    ".#.#....#.#.",
    ".#..####..#.",
    ".#........#.",
    "..########..",
    "............",
    "............",
};
static const char *const *const art[ICON_COUNT] = {run, pause, step, step_over, reset, breakpoint, screenshot};

// Expands one icon into an RGBA buffer of ICON_SIZE*ICON_SIZE*4 bytes owned by the
// caller. Re-expanding the same buffer with a different tint is how icons follow a
// theme change; no image library (libfltk_images) is required.
inline void expand(Icon which, Fl_Color tint, unsigned char *pixels)
{
    unsigned char r, g, b;
    Fl::get_color(tint, r, g, b);
    for (int y = 0; y < ICON_SIZE; y++)
        for (int x = 0; x < ICON_SIZE; x++) {
            bool on = art[which][y][x] == '#';
            unsigned char *p = pixels + (y * ICON_SIZE + x) * 4;
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = on ? 255 : 0;
        }
}
} // namespace zeal_icons
