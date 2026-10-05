// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "debugger/frontend.h"
#include "ui/fltk/dock.h"
#include "ui/fltk/glyph_font.h"
#include "ui/fltk/icons.h"
#include "ui/fltk/theme.h"
#include <FL/Fl.H>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Fl_Box;
class Fl_Button;
class Fl_Double_Window;
class Fl_Flex;
class Fl_Menu_Bar;
class Fl_RGB_Image;

// Forward declarations: the shell owns panels and the dock surface, but the widgets and
// panels include this header, so the dependency only ever points this way.
class DockSurface;
class Panel;

namespace zeal_ui
{
enum PanelId { PANEL_VIDEO, PANEL_CPU, PANEL_BREAKPOINTS, PANEL_DISASSEMBLER,
               PANEL_MEMORY, PANEL_MMU, PANEL_SEMIHOST, PANEL_VRAM, PANEL_COUNT };
// Human-readable panel title, indexed by PanelId.
const char *panel_name(int id);
}

// Segmented status bar fields, left to right.
enum StatusField { ST_STATE, ST_PC, ST_FRAME, ST_FPS, ST_MESSAGE, ST_MACHINE, ST_COUNT };
// Toolbar buttons, left to right. The first five map 1:1 onto dbg_command_t.
enum ToolbarButton { TB_RUN, TB_PAUSE, TB_STEP, TB_STEP_OVER, TB_RESET, TB_BREAKPOINT, TB_SCREENSHOT,
                     TB_COUNT };

// The debugger window shell. It owns the menu, toolbar, status bar, dock workspace and
// the floating panel windows, and it is the object the C entry points exchange with the
// rest of the emulator. Members stay public because the smoke test drives them directly.
struct dbg_ui_t {
    dbg_ui_init_args_t host;
    Fl_Double_Window *window = nullptr;
    Fl_Menu_Bar *menu = nullptr;
    Fl_Flex *shell = nullptr, *toolbar_row = nullptr;
    Fl_Flex *status_bar = nullptr;
    std::array<Fl_Box *, ST_COUNT> status_fields{};
    std::vector<Fl_Box *> status_separators;
    Fl_Box *status_dot = nullptr;
    uint64_t status_frames = 0;
    double status_frames_time = 0, status_fps = 0;
    int main_x = 0, main_y = 0, main_w = 0, main_h = 0;
    bool main_geometry_saved = false;
    std::array<Fl_Button *, TB_COUNT> toolbar{};
    std::array<Fl_RGB_Image *, zeal_icons::ICON_COUNT> icons{};
    std::array<unsigned char *, zeal_icons::ICON_COUNT> icon_pixels{};
    uint64_t event_sequence = 0;
    DockSurface *surface = nullptr;
    std::array<Panel *, zeal_ui::PANEL_COUNT> panels{};
    std::map<int, Fl_Double_Window *> floating;
    zeal_ui::Workspace workspace;
    zeal_ui::Theme theme = zeal_ui::Theme::preset(false);
    GlyphFont glyph_font;
    std::string theme_name = "Dark", directory;
    Fl_Font ui_font = FL_HELVETICA, mono_font = FL_COURIER;
    bool shown = false, passthrough = false, upper = true, cp437 = false, rebuild = false;
    bool separate_mode = false, menu_dirty = false;
    int scale = 1, volume = 100, drag = -1, drag_x = 0, drag_y = 0;
    double last_refresh = 0;
    dbg_snapshot_t snapshot{};
    std::string message;
    dbg_snes_state_t snes{};
    struct SnesChoice {
        dbg_ui_t *ui;
        int port, device;
    };
    std::vector<SnesChoice> snes_choices;

    Fl_Color color(const char *role) const
    {
        unsigned rgb = theme.colors.at(role);
        return fl_rgb_color(rgb >> 16, (rgb >> 8) & 255, rgb & 255);
    }

    void apply_theme();
    void select_theme(const std::string &);
    void build_menu();
    void layout_status();
    void apply_icons();
    void screenshot();
    bool separate_windows() const;
    void set_separate_windows(bool on);
    void detach_panel(int id);
    bool panel_open(int id) const;
    void refresh_snes();
    void layout();
    void changed_layout();
    void save();
    void hide_panel(int id);
    void drop(int x, int y);
    void command(dbg_command_t c)
    {
        debugger_command(host.debugger, c);
        last_refresh = 0;
    }

    void release();
    void navigate(uint32_t addr);
    void show_window(bool visible);
};

namespace zeal_ui
{
// Builds the whole shell (window, chrome, panels, workspace) for the C entry point.
int create_debugger_ui(dbg_ui_t **out, const dbg_ui_init_args_t *args);
void destroy_debugger_ui(dbg_ui_t *u);
} // namespace zeal_ui
// Readable alias for new C++ code; the C API keeps exchanging dbg_ui_t pointers.
using DebuggerUi = dbg_ui_t;
