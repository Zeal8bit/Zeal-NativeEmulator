// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl_Table.H>
#include <cstdint>
#include <string>
#include <vector>

struct dbg_ui_t;
class Fl_Menu_Button;

// What a ListTable shows. Every panel that displays a table implements this interface,
// so the table itself never learns which panel is using it: no column counts, headers,
// cell contents or actions are chosen by panel id anywhere.
class ListSource
{
  public:
    // Actions the shared context menu and keyboard can trigger.
    enum Action { ACT_REMOVE, ACT_TOGGLE_BREAK, ACT_GOTO, ACT_COPY, ACT_TOGGLE_COUNTER };

    virtual ~ListSource() = default;
    virtual int row_count() const = 0;
    virtual int column_count() const = 0;
    virtual const char *column_header(int column) const = 0;
    // One width per column. `table_width` lets the flexible column take the slack.
    virtual void column_widths(int *out, int table_width) const = 0;
    // Text for a cell; `role` is a theme role the source may narrow.
    virtual std::string cell_text(int row, int column, const char *&role) const = 0;
    // Background role for a row, or nullptr to use the default. The selection wins.
    virtual const char *row_background(int) const
    {
        return nullptr;
    }

    // Address the row stands for, or 0 when it has none.
    virtual uint32_t row_address(int row) const = 0;
    // True when address 0 is a real target, so navigating to it must still happen.
    virtual bool navigates_to_zero() const
    {
        return false;
    }

    // Context menu: the shared Action ids, in display order.
    virtual int action_count() const = 0;
    virtual int action_id(int index) const = 0;
    virtual const char *action_label(int index) const = 0;
    // Action run by Enter, Delete and Space.
    virtual int keyboard_action() const = 0;
    // Action run by a double-click, and by a single click when the source asks for it.
    virtual int default_action() const = 0;
    virtual bool acts_on_single_click() const
    {
        return false;
    }

    // Hint drawn under an empty table, or nullptr.
    virtual const char *empty_hint() const
    {
        return nullptr;
    }

    // Handles every action except the two the table implements itself (go to, copy).
    virtual void run_action(int action, int row) = 0;
    // Lets the panel enable or disable its own controls when the selection changes.
    virtual void selection_changed(int, int)
    {
    }
};

class ListTable : public Fl_Table
{
  public:
    struct MenuAction {
        ListTable *table;
        int action;
    };
    ListSource *source;
    dbg_ui_t *ui;
    int sel_row = 0, menu_row = -1, row_metric = 0, row_count_shown = -1;
    Fl_Menu_Button *menu = nullptr;
    std::vector<MenuAction> menu_actions;

    ListTable(ListSource *source, dbg_ui_t *ui, int x, int y, int w, int h);
    int list_rows() const;
    int list_columns() const;
    int selected_row() const
    {
        return sel_row;
    }

    void sync();
    void select_row(int row);
    void reveal(int row);
    void context_menu(int row);
    void run_action(int action);
    void go_to(int row);
    void remove_selected();
    static void action_cb(Fl_Widget *w, void *data);
    void draw_cell(TableContext context, int R = 0, int C = 0, int X = 0, int Y = 0, int W = 0,
                   int H = 0) override;
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;
};
