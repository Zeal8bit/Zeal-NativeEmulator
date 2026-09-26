// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <map>
#include <string>
namespace zeal_ui {
struct Theme {
    std::map<std::string,unsigned> colors;
    std::string ui_font="Helvetica", mono_font="Courier";
    int font_size=13, mono_size=13, spacing=6, row_height=24;
    static Theme preset(bool light);
    static bool load(const std::string& file, Theme& out, std::string& error);
};
}
