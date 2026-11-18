/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "symbols/map_files.h"
#include <cstddef>
#include <string>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/filename.H>
#include "common/machine.h"
#include "debugger/navigation.h"
#include "frontend/sdl-fltk/bridge/symbols.h"
#include "tools/tool_windows.h"

namespace zui::symbols {

using namespace zui::debugger;
using namespace zui::tools;

namespace {

/* Every view reads the symbol table live; just redraw everything */
void symbols_changed()
{
    request_refresh();
    tool_windows_refresh(fz_paused(), true);
}

} // namespace

bool map_open_dialog(bool append)
{
    static std::string s_dir;
    Fl_Native_File_Chooser chooser;
    chooser.title(append ? "Add Map File" : "Open Map File");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("Map files\t*.map\nAll files\t*");
    /* Start in the folder picked last time, else the HostFS folder, where
     * the programs being tested (and their maps) usually live */
    std::string start = s_dir;
    if (start.empty() && config.arguments.hostfs_path) {
        char abs[FL_PATH_MAX];
        fl_filename_absolute(abs, sizeof(abs), config.arguments.hostfs_path);
        start = abs;
    }
    if (!start.empty()) chooser.directory(start.c_str());
    if (chooser.show() != 0 || chooser.filename() == nullptr) return false;
    const std::string path = chooser.filename();
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) s_dir = path.substr(0, slash);

    const int before = fz_symbol_count();
    if (!fz_map_open(path.c_str(), append)) {
        notif_show("Could not load %s", path.c_str());
        return false;
    }
    const int loaded = append ? fz_symbol_count() - before : fz_symbol_count();
    const char* name = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
    notif_show("%d symbols loaded from %s", loaded, name);
    symbols_changed();
    return true;
}

void map_reload()
{
    const int files = fz_map_count();
    if (files == 0) return;
    const int ok = fz_map_reload();
    if (ok == files) notif_show("Reloaded %d symbols", fz_symbol_count());
    else notif_show("Reloaded %d of %d map files", ok, files);
    symbols_changed();
}

void map_clear()
{
    fz_map_clear();
    notif_show("Symbols cleared");
    symbols_changed();
}

} // namespace zui::symbols
