// SPDX-License-Identifier: Apache-2.0
// The C entry points the emulator calls into. Everything these functions need lives in
// the shell (ui/fltk/debugger_ui.cpp), the panels (ui/fltk/panels/) and the reusable
// widgets (ui/fltk/widgets/); this file only adapts between the C API and that code.
#include "debugger/frontend.h"
#include "ui/fltk/debugger_ui.h"
#include "ui/fltk/panel.h"
#include "ui/fltk/util.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <algorithm>
#include <cstdio>
#include <string>

using zeal_ui::hex;
using zeal_ui::now;

void debugger_ui_smoke_tick(dbg_ui_t *u);

extern "C" int debugger_ui_init(dbg_ui_t **out, const dbg_ui_init_args_t *args)
{
    return zeal_ui::create_debugger_ui(out, args);
}

extern "C" void debugger_ui_deinit(dbg_ui_t *u)
{
    zeal_ui::destroy_debugger_ui(u);
}

extern "C" void debugger_ui_show(dbg_ui_t *u, bool visible)
{
    if (u)
        u->show_window(visible);
}

extern "C" void debugger_ui_set_debugging(dbg_ui_t *u, bool on)
{
    if (u)
        u->set_debugging(on);
}

extern "C" void debugger_ui_poll(dbg_ui_t *u)
{
    if (!u)
        return;
#ifdef CONFIG_FLTK_TESTS
    if (!u->shown)
        debugger_ui_smoke_tick(u);
#endif
    if (!u->shown)
        return;
    u->refresh_snes();
    Fl::check();
    if (u->rebuild)
        u->layout(); // may mark the View check marks stale
    if (u->menu_dirty && !Fl::grab()) {
        u->build_menu();
        u->menu_dirty = false;
    }
    debugger_ui_refresh(u);
#ifdef CONFIG_FLTK_TESTS
    debugger_ui_smoke_tick(u);
#endif
    if (u->snapshot.paused)
        Fl::wait(.005);
}

extern "C" void debugger_ui_refresh(dbg_ui_t *u)
{
    if (!u || !u->shown)
        return;
    double time = now();
    if (time - u->last_refresh < 1.0 / 30)
        return;
    u->last_refresh = time;
    debugger_snapshot(u->host.debugger, &u->snapshot);
    dbg_event_record_t events[64];
    uint32_t count = 0, overflow = 0;
    debugger_events(u->host.debugger, u->event_sequence, events, 64, &count, &overflow);
    for (uint32_t i = 0; i < count; i++) {
        u->event_sequence = events[i].sequence;
        if (events[i].reason == DBG_REASON_BREAKPOINT || events[i].reason == DBG_REASON_WATCHPOINT)
            u->message = std::string(events[i].reason == DBG_REASON_BREAKPOINT ? "Breakpoint at $"
                                                                               : "Watchpoint at $") +
                         hex(events[i].address, 4);
    }
    if (overflow)
        u->message = "Some debugger events expired; current state refreshed";
    for (Panel *p : u->panels)
        if (p->visible_r())
            p->refresh();
    dbg_render_stats_t stats{};
    debugger_render_stats(u->host.debugger, &stats);
    double elapsed = time - u->status_frames_time;
    if (elapsed > 0.4) {
        u->status_fps = double(stats.frames - u->status_frames) / elapsed;
        u->status_frames = stats.frames;
        u->status_frames_time = time;
    }
    char notification[128] = {};
    if (u->host.notification)
        u->host.notification(notification, sizeof(notification));
    std::string message = u->message;
    if (*notification) {
        if (!message.empty())
            message += "    ";
        message += notification;
    }
    char fps[32];
    std::snprintf(fps, sizeof(fps), "%.2f fps", u->status_fps);
    u->status_fields[ST_STATE]->copy_label(u->snapshot.paused ? "Paused" : "Running");
    u->status_fields[ST_STATE]->labelcolor(u->color(u->snapshot.paused ? "paused" : "success"));
    u->status_fields[ST_PC]->copy_label(("PC $" + hex(u->snapshot.pc, 4)).c_str());
    u->status_fields[ST_FRAME]->copy_label(("Frame " + std::to_string(stats.frames)).c_str());
    u->status_fields[ST_FPS]->copy_label(fps);
    u->status_fields[ST_MESSAGE]->copy_label(message.c_str());
    u->status_fields[ST_MACHINE]->copy_label("Zeal 8-bit Computer");
}

extern "C" bool debugger_ui_main_view_focused(const dbg_ui_t *u)
{
    return u && u->shown && u->panels[zeal_ui::PANEL_VIDEO]->input_focused();
}

extern "C" dbg_vram_t debugger_ui_vram_panel_opened(const dbg_ui_t *u)
{
    return u && u->shown && u->panels[zeal_ui::PANEL_VRAM]->visible_r()
               ? (dbg_vram_t)u->panels[zeal_ui::PANEL_VRAM]->selected_view()
               : DBG_VIEW_NONE;
}

extern "C" void debugger_ui_scale(dbg_ui_t *u, int delta)
{
    if (u)
        u->scale = std::clamp(u->scale + delta, 1, 6);
}
