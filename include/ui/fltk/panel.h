// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/debugger_ui.h"
#include <cstdint>
#include <FL/Fl_Flex.H>

class ListTable;

// One dockable debugger panel. A subclass owns its widgets and its state, and the shell
// only ever talks to this interface: nothing outside a panel needs to know which panel
// it is looking at, so there is no "if this is panel N" anywhere in the shell.
class Panel : public Fl_Flex
{
  public:
    dbg_ui_t *ui;
    int id;

    Panel(dbg_ui_t *u, int id);
    // Re-reads the debugger state this panel displays.
    virtual void refresh() = 0;

    // The Video panel can grab the mouse and swallow guest input; top-level windows
    // forward raw events here while that is the case.
    virtual bool captures_input() const
    {
        return false;
    }

    virtual int forward_input(int)
    {
        return 0;
    }

    virtual void release_capture()
    {
    }

    // True while this panel holds the keyboard focus, which is what feeds guest input.
    virtual bool input_focused() const
    {
        return false;
    }

    // Selected view for panels that have a view chooser, DBG_VIEW_NONE otherwise.
    virtual int selected_view() const
    {
        return DBG_VIEW_NONE;
    }

    // The table a list panel shows, or nullptr. Used by the smoke test to walk the
    // tables without knowing which panel each one belongs to.
    virtual ListTable *list_table() const
    {
        return nullptr;
    }

    // Points the panel at an address, for panels that show one. No-op elsewhere.
    virtual void go_to_address(uint32_t)
    {
    }

    void resize(int x, int y, int w, int h) override;

  protected:
    // Closes the panel's flex and hides it. Every subclass constructor ends with this.
    void finish();
};
