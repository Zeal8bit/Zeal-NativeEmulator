// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/control_row.h"
#include "ui/fltk/widgets/list_table.h"
#include <cstdint>
#include <utility>
#include <vector>

// The instruction listing, tracked to the current PC while the CPU is paused.
class DisassemblerPanel : public Panel, public ListSource
{
  public:
    explicit DisassemblerPanel(dbg_ui_t *u);
    void refresh() override;
    void resize(int x, int y, int w, int h) override;

    int row_count() const override
    {
        return int(instructions.size());
    }

    int column_count() const override
    {
        return 4;
    }

    const char *column_header(int column) const override;
    void column_widths(int *out, int table_width) const override;
    std::string cell_text(int row, int column, const char *&role) const override;
    const char *row_background(int row) const override;
    uint32_t row_address(int row) const override;
    int action_count() const override
    {
        return 3;
    }

    int action_id(int index) const override;
    const char *action_label(int index) const override;
    int keyboard_action() const override
    {
        return ACT_TOGGLE_BREAK;
    }

    int default_action() const override
    {
        return ACT_GOTO;
    }

    void run_action(int action, int row) override;

    ListTable *list_table() const override
    {
        return table;
    }

    std::vector<std::pair<uint32_t, dbg_instr_t>> instructions;
    uint32_t start = 0;

  private:
    ControlRow *controls = nullptr;
    ListTable *table = nullptr;
    uint32_t shown_pc = 0xffffffff;
};
