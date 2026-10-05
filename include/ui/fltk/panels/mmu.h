// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/list_table.h"

// The four virtual-to-physical mappings.
class MmuPanel : public Panel, public ListSource
{
  public:
    explicit MmuPanel(dbg_ui_t *u);
    void refresh() override;

    int row_count() const override
    {
        return 4;
    }

    int column_count() const override
    {
        return 4;
    }

    const char *column_header(int column) const override;
    void column_widths(int *out, int table_width) const override;
    std::string cell_text(int row, int column, const char *&role) const override;
    uint32_t row_address(int row) const override;
    bool navigates_to_zero() const override
    {
        return true;
    }

    int action_count() const override
    {
        return 2;
    }

    int action_id(int index) const override;
    const char *action_label(int index) const override;
    int keyboard_action() const override
    {
        return ACT_GOTO;
    }

    int default_action() const override
    {
        return ACT_GOTO;
    }

    bool acts_on_single_click() const override
    {
        return true;
    }

    void run_action(int, int) override
    {
    }

    ListTable *list_table() const override
    {
        return table;
    }

    dbg_mapping_t mappings[4]{};

  private:
    ListTable *table = nullptr;
};
