// SPDX-License-Identifier: Apache-2.0
// Bounded smoke test for the desktop UI. Built only with -Dfltk_test_hooks=true and
// driven by tools/test_fltk.py; it walks the panels, the workspace and the menus and
// captures screenshots, so a change that breaks the shell fails the build test rather
// than only being noticed by hand.
#include "debugger/frontend.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/panel.h"
#include "ui/fltk/panels/cpu.h"
#include "ui/fltk/panels/memory.h"
#include "ui/fltk/panels/video.h"
#include "ui/fltk/panels/vram.h"
#include "ui/fltk/util.h"
#include "ui/fltk/widgets/canvas.h"
#include "ui/fltk/widgets/list_table.h"
#include "ui/fltk/widgets/memory_table.h"
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include "../../tests/fltk_popup_checks.h"

using zeal_ui::hex;
using zeal_ui::now;

#ifdef CONFIG_FLTK_TESTS
void debugger_ui_smoke_tick(dbg_ui_t *u)
{
    const char *path = std::getenv("ZEAL_FLTK_SMOKE");
    if (!path)
        return;
    static double started = now();
    static int phase = 0;
    static dbg_snes_state_t original_snes{};
    double elapsed = now() - started;
    auto capture = [&](const char *suffix) {
        u->last_refresh = 0;
        debugger_ui_refresh(u);
        u->window->make_current();
        Fl::flush();
        uchar *pixels = fl_read_image(nullptr, 0, 0, u->window->w(), u->window->h(), 0);
        assert(pixels);
        std::ofstream f(std::string(path) + suffix + ".ppm", std::ios::binary);
        f << "P6\n" << u->window->w() << " " << u->window->h() << "\n255\n";
        f.write((char *)pixels, u->window->w() * u->window->h() * 3);
        delete[] pixels;
        assert(f.good());
    };
    auto pick = [&](const char *label) {
        const Fl_Menu_Item *item = u->menu->find_item(label);
        assert(item);
        u->menu->picked(item);
    };
    if (phase == 0 && elapsed > .2) {
        fltk_check_menu_placement(*u->window, *u->menu);
        // Every View toggle must describe the workspace loaded from disk. Building the
        // menu before the workspace was loaded left the check marks describing the
        // built-in default layout instead.
        for (int i = 0; i < 8; i++) {
            const Fl_Menu_Item *item = u->menu->find_item((std::string("View/") + zeal_ui::panel_name(i)).c_str());
            assert(item);
            assert(bool(item->value()) == u->panel_open(i));
        }
        pick("View/Breakpoints"); // hidden by the seeded workspace
        assert(u->panel_open(2));
        /* The CP437 atlas comes from the host (FreeType under FLTK, Raylib for the
         * web build); a host that fails to rasterize would blank the memory column. */
        assert(u->glyph_font.ready());
        const int original_x = u->window->x(), original_y = u->window->y();
        for (int screen = 0; screen < Fl::screen_count(); ++screen) {
            int x, y, width, height;
            Fl::screen_work_area(x, y, width, height, screen);
            u->window->position(x + (width - u->window->w()) / 2, y + (height - u->window->h()) / 2);
            Fl::check();
            fltk_check_menu_placement(*u->window, *u->menu);
        }
        u->window->position(original_x, original_y);
        Fl::check();
        assert(!u->menu->find_item("SNES/Controller 3") && !u->menu->find_item("SNES/Port 3"));
        u->host.snes_state(u->host.debugger, &original_snes);
        pick("SNES/Port 1/Emulated SNES Mouse");
        u->refresh_snes();
        assert(u->snes.ports[0] == DBG_SNES_MOUSE && u->snes.ports[1] != DBG_SNES_MOUSE);
        assert(u->menu->find_item("SNES/Port 1/Emulated SNES Mouse")->value());
        pick("SNES/Port 2/Emulated SNES Mouse");
        u->refresh_snes();
        assert(u->snes.ports[0] == DBG_SNES_DETACHED && u->snes.ports[1] == DBG_SNES_MOUSE);
        pick("SNES/Port 2/Detached");
        u->refresh_snes();
        // Releasing guest keys must not cancel a popup's own FLTK grab.
        Fl::grab(u->window);
        u->release();
        assert(Fl::grab() == u->window);
        Fl::grab(nullptr);
        VideoPanel *video = static_cast<VideoPanel *>(u->panels[zeal_ui::PANEL_VIDEO]);
        video->canvas->captured = true;
        Fl::grab(video->canvas->window());
        video->canvas->handle(FL_UNFOCUS);
        assert(!Fl::grab() && !video->canvas->captured);
        pick("Video/Scale Up");
        assert(u->scale == 2);
        pick("Video/Scale Down");
        assert(u->scale == 1);
        pick("CPU/Continue");
        phase++;
    } else if (phase == 1 && elapsed > 1) {
        u->refresh_snes();
        // The menu was rebuilt after the toggle, so the mark now matches the new state.
        assert(u->menu->find_item("View/Breakpoints")->value());
        assert(u->snes.ports[0] == DBG_SNES_DETACHED && u->snes.ports[1] == DBG_SNES_DETACHED);
        for (int port = 0; port < DBG_SNES_PORTS; ++port)
            u->host.action(u->host.debugger, UI_SNES_PORT, port, original_snes.ports[port]);
        u->refresh_snes();
        pick("CPU/Pause");
        u->workspace.detach(6, 50, 50);
        u->changed_layout();
        phase++;
    } else if (phase == 2 && elapsed > 1.2) {
        assert(u->floating.count(6));
        assert(u->floating.at(6)->screen_num() ==
               Fl::screen_num(u->floating.at(6)->x(), u->floating.at(6)->y(), u->floating.at(6)->w(),
                              u->floating.at(6)->h()));
        u->workspace.dock(6, 5, 0);
        u->changed_layout();
        pick("Theme/Light");
        assert(u->theme_name == "Light");
        phase++;
    } else if (phase == 3 && elapsed > 1.4) {
        assert(u->workspace.leaf(6) == u->workspace.leaf(5));
        capture("-light");
        pick("View/VRAM");
        assert(u->workspace.leaf(7));
        phase++;
    } else if (phase >= 4 && phase <= 8 && elapsed > 1.6 + (phase - 4) * .2) {
        VramPanel *vram = static_cast<VramPanel *>(u->panels[zeal_ui::PANEL_VRAM]);
        vram->select_view(phase - 4);
        vram->refresh();
        assert(!vram->canvas->pixels.empty());
        assert(vram->canvas->image.width > 0);
        vram->canvas->hover = phase == 7 ? 12 : 65;
        capture(("-vram" + std::to_string(phase - 4)).c_str());
        phase++;
    } else if (phase == 9 && elapsed > 2.8) {
        std::ofstream f(u->directory + "/themes/smoke.ini");
        f << "version=1\nbase=Light\nfont_size=18\nmono_size=18\nrow_height=30\n";
        f.close();
        u->select_theme("smoke.ini");
        assert(u->theme.font_size == 18);
        u->cp437 = true;
        phase++;
    } else if (phase == 10 && elapsed > 3) {
        capture("-large-font");
        u->workspace.reset();
        u->changed_layout();
        u->select_theme("Dark");
        u->command(DBG_STEP);
        phase++;
    } else if (phase == 11 && elapsed > 3.2) {
        pick("File/Debugger Off");
        phase++;
    } else if (phase == 12 && elapsed > 3.4) {
        // Switching the debugger off keeps the shell on screen: only the debug panels
        // go away, leaving the video filling the window.
        assert(u->shown);
        assert(u->panel_open(zeal_ui::PANEL_VIDEO));
        assert(!u->panel_open(zeal_ui::PANEL_CPU));
        assert(!u->panel_open(zeal_ui::PANEL_MEMORY));
        // The classic view is the menu bar over the video: no toolbar, no status bar,
        // and a menu with the emulator's own entries instead of the debugger's.
        assert(!u->toolbar_row->visible());
        assert(!u->status_bar->visible());
        assert(u->menu->find_item("Machine/Reset"));
        assert(u->menu->find_item("View/Scale Up"));
        assert(u->menu->find_item("Debugger/Toggle Debugger"));
        assert(u->menu->find_item("Help/About"));
        assert(!u->menu->find_item("CPU/Pause"));
        assert(!u->menu->find_item("View/Breakpoints"));
        capture("-classic");
        // Turning the debugger back on goes through the classic menu, not the host API.
        pick("Debugger/Toggle Debugger");
        phase++;
    } else if (phase == 13 && elapsed > 3.6) {
        assert(u->shown);
        // The layout the debugger had before it was switched off comes back, chrome and
        // all.
        assert(u->panel_open(zeal_ui::PANEL_CPU));
        assert(u->panel_open(zeal_ui::PANEL_MEMORY));
        assert(u->toolbar_row->visible());
        assert(u->status_bar->visible());
        assert(u->menu->find_item("CPU/Pause"));
        assert(!u->menu->find_item("Machine/Reset"));
        u->command(DBG_CONTINUE);
        phase++;
    } else if (phase == 14 && elapsed > 3.8) {
        u->command(DBG_PAUSE);
        phase++;
    } else if (phase == 15 && elapsed > 4) {
        assert(u->snapshot.paused);
        assert(static_cast<VideoPanel *>(u->panels[zeal_ui::PANEL_VIDEO])->canvas->image.generation > 5);
        {
            // Memory editing: a steady view must not request repaints, RAM writes
            // land, and a write the device ignores must not be shown as applied.
            MemoryPanel *p = static_cast<MemoryPanel *>(u->panels[zeal_ui::PANEL_MEMORY]);
            MemoryTable *t = p->memory_table();
            p->start = 0x8000;
            p->range = 256;
            u->last_refresh = 0;
            p->refresh();
            assert(!t->refresh_view(p->memory.size(), p->start, u->upper, u->cp437));
            uint8_t original = p->memory[3];
            assert(t->write_byte(3, 0x5A));
            uint8_t readback = 0;
            debugger_memory_read(u->host.debugger, DBG_VIRTUAL, 0x8003, &readback, 1);
            assert(readback == 0x5A && p->memory[3] == 0x5A);
            assert(t->write_byte(3, original));
            p->start = 0;
            u->last_refresh = 0;
            p->refresh();
            uint8_t rom = p->memory[0];
            assert(t->write_byte(0, uint8_t(rom ^ 0xFF)));
            assert(p->memory[0] == rom);
            u->message.clear();
            // Selection spans whole rows and must cover every affected row, which is
            // what the old Fl_Table column-based damage got wrong.
            t->set_cursor(3, 1);
            assert(t->sel_lo == 0 && t->sel_hi == 0);
            t->set_cursor(3 + 2 * t->shown_columns, 1, true);
            assert(t->sel_lo == 0 && t->sel_hi == 2);
            // Focus the table so the capture also covers caret rendering.
            t->set_cursor(3, 1);
            t->take_focus();
        }
        // The toolbar carries each command in argument(), which aliases user_data();
        // exercising it here guards against that storage collision coming back.
        u->toolbar[1]->do_callback(); // Pause
        assert(u->snapshot.paused);
        // The list panels are Fl_Tables now: header and row counts must stay in step.
        for (int id : {zeal_ui::PANEL_BREAKPOINTS, zeal_ui::PANEL_DISASSEMBLER, zeal_ui::PANEL_MMU,
                       zeal_ui::PANEL_SEMIHOST}) {
            ListTable *t = u->panels[id]->list_table();
            assert(t);
            t->sync();
            assert(t->rows() == t->list_rows());
            assert(t->cols() == t->list_columns());
            assert(t->list_rows() >= 0);
        }
        assert(u->panels[zeal_ui::PANEL_DISASSEMBLER]->list_table()->list_rows() > 0);
        // Register fields must stay wide enough to show four hex digits. Fl_Grid's
        // default column weight of 50 once squeezed them to 2px, which looked like the
        // fields had disappeared entirely.
        {
            fl_font(u->mono_font, u->theme.mono_size);
            int need = int(fl_width("0000"));
            CpuPanel *cpu = static_cast<CpuPanel *>(u->panels[zeal_ui::PANEL_CPU]);
            for (int i = 0; i < 14; i++)
                assert(cpu->registers[i]->w() >= need);
        }
        capture("");
        u->save();
        dbg_render_stats_t stats{};
        debugger_render_stats(u->host.debugger, &stats);
        std::fprintf(stdout, "FLTK_SMOKE_OK pc=%04x frame=%llu copy_average_us=%llu copy_max_us=%llu\n",
                     u->snapshot.pc, (unsigned long long)stats.frames,
                     (unsigned long long)(stats.frames ? stats.total_copy_ns / stats.frames / 1000 : 0),
                     (unsigned long long)(stats.max_copy_ns / 1000));
        u->command(DBG_STOP);
        phase++;
    }
}

#endif
