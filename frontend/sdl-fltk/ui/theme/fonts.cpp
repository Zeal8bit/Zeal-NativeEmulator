/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/fonts.h"
#include <cstdio>
#include <cstring>
#include <FL/Fl.H>
#include "theme/theme.h"

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(ZUI_HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

extern "C" {
#include "utils/paths.h"
}

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

namespace zui::theme {

Fl_Font DATA_FONT = FL_COURIER;
int DATA_SIZE = 13;
bool g_pixel_font = false;

static const char* FONT_FILE = "BigBlue_Terminal_437TT.TTF";
static const char* FONT_FAMILY = "BigBlue Terminal 437TT";

static bool register_font_file(const char* path)
{
#if defined(__APPLE__)
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    if (url == nullptr) return false;
    const bool ok = CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
    CFRelease(url);
    return ok;
#elif defined(_WIN32)
    return AddFontResourceExA(path, FR_PRIVATE, 0) > 0;
#elif defined(ZUI_HAVE_FONTCONFIG)
    return FcConfigAppFontAddFile(nullptr, (const FcChar8*) path) == FcTrue;
#else
    (void) path;
    return false;
#endif
}

void load_fonts()
{
    char exe_dir[PATH_MAX];
    char path[PATH_MAX + 64];
    get_executable_dir(exe_dir, sizeof(exe_dir));

    const char* candidates[3];
    char dev[PATH_MAX + 64];
    char inst[PATH_MAX + 64];
    snprintf(dev, sizeof(dev), "%s/assets/fonts/%s", exe_dir, FONT_FILE);
    snprintf(inst, sizeof(inst), "%s/fonts/%s", ZEAL_ASSETS_DIR, FONT_FILE);
    candidates[0] = dev;
    candidates[1] = inst;
    candidates[2] = nullptr;

    for (int i = 0; candidates[i]; i++) {
        snprintf(path, sizeof(path), "%s", candidates[i]);
        if (!path_exists(path)) continue;
        if (register_font_file(path)) {
            Fl::set_font(FL_FREE_FONT, FONT_FAMILY);
            DATA_FONT = FL_FREE_FONT;
            DATA_SIZE = 16; /* the font's native 8x16 cell */
            g_pixel_font = true;
            return;
        }
    }
    fprintf(stderr, "[UI] %s not found, using a system monospace font\n", FONT_FILE);
}

void apply_ui_font()
{
    /* Use the system monospace font for UI text (labels, buttons, menus,
     * tooltips, status bar, overlays) by remapping FLTK's default sans faces
     * onto the courier slots, after pointing those at a good system mono. */
#if defined(__APPLE__)
    Fl::set_font(FL_COURIER, "Menlo-Regular");
    Fl::set_font(FL_COURIER_BOLD, "Menlo-Bold");
    Fl::set_font(FL_COURIER_ITALIC, "Menlo-Italic");
    Fl::set_font(FL_COURIER_BOLD_ITALIC, "Menlo-BoldItalic");
#elif defined(_WIN32)
    Fl::set_font(FL_COURIER, " Consolas");
    Fl::set_font(FL_COURIER_BOLD, "BConsolas");
    Fl::set_font(FL_COURIER_ITALIC, "IConsolas");
    Fl::set_font(FL_COURIER_BOLD_ITALIC, "PConsolas");
#endif
    /* Linux/BSD: FLTK's courier slots already resolve to fontconfig "mono" */
    Fl::set_font(FL_HELVETICA, FL_COURIER);
    Fl::set_font(FL_HELVETICA_BOLD, FL_COURIER_BOLD);
    Fl::set_font(FL_HELVETICA_ITALIC, FL_COURIER_ITALIC);
    Fl::set_font(FL_HELVETICA_BOLD_ITALIC, FL_COURIER_BOLD_ITALIC);
}

} // namespace zui::theme
