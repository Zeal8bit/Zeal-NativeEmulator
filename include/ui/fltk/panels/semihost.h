// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/list_table.h"

// The eight semihost counters, each of which can break on update.
class SemihostPanel : public Panel, public ListSource
{
  public:
    explicit SemihostPanel(dbg_ui_t *u);
    void refresh() override;

    int row_count() const override
    {
        return 8;
    }

    int column_count() const override
    {
        return 7;
    }

    const char *column_header(int column) const override;
    void column_widths(int *out, int table_width) const override;
    std::string cell_text(int row, int column, const char *&role) const override;
    uint32_t row_address(int) const override
    {
        return 0;
    }

    int action_count() const override
    {
        return 1;
    }

    int action_id(int) const override
    {
        return ACT_TOGGLE_COUNTER;
    }

    const char *action_label(int) const override
    {
        return "Toggle break on update";
    }

    int keyboard_action() const override
    {
        return ACT_TOGGLE_COUNTER;
    }

    int default_action() const override
    {
        return ACT_TOGGLE_COUNTER;
    }

    bool acts_on_single_click() const override
    {
        return true;
    }

    void run_action(int action, int row) override;

    ListTable *list_table() const override
    {
        return table;
    }

    dbg_counter_t counters[8]{};

  private:
    ListTable *table = nullptr;
};
