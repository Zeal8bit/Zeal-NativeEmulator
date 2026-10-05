// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/control_row.h"
#include "ui/fltk/widgets/list_table.h"
#include <array>
#include <vector>

// Breakpoints and watchpoints, with the add/edit/remove buttons under the table.
class BreakpointsPanel : public Panel, public ListSource
{
  public:
    explicit BreakpointsPanel(dbg_ui_t *u);
    void refresh() override;
    void resize(int x, int y, int w, int h) override;

    // ListSource
    int row_count() const override;
    int column_count() const override
    {
        return 3;
    }

    const char *column_header(int column) const override;
    void column_widths(int *out, int table_width) const override;
    std::string cell_text(int row, int column, const char *&role) const override;
    uint32_t row_address(int row) const override;
    int action_count() const override
    {
        return 3;
    }

    int action_id(int index) const override;
    const char *action_label(int index) const override;
    int keyboard_action() const override
    {
        return ACT_REMOVE;
    }

    int default_action() const override
    {
        return ACT_GOTO;
    }

    const char *empty_hint() const override
    {
        return "Enter an address above to add a breakpoint";
    }

    void run_action(int action, int row) override;
    void selection_changed(int row, int rows) override;

    ListTable *list_table() const override
    {
        return table;
    }

    void add_entry();
    void edit_entry();

    std::vector<hwaddr> breakpoints;
    std::vector<watchpoint_t> watchpoints;

  private:
    void add_at(uint32_t addr);
    void add_from_controls();
    ControlRow *controls = nullptr;
    ListTable *table = nullptr;
    Fl_Flex *actions = nullptr;
    std::array<Fl_Button *, 3> action_buttons{};
};
