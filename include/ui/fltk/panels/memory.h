// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/control_row.h"
#include "ui/fltk/widgets/memory_table.h"
#include <cstdint>
#include <vector>

// The hex view. Editing writes through the debugger so a device that ignores a write is
// reported rather than shown as applied.
class MemoryPanel : public Panel, public MemorySource
{
  public:
    explicit MemoryPanel(dbg_ui_t *u);
    void refresh() override;
    void resize(int x, int y, int w, int h) override;

    uint32_t memory_start() const override
    {
        return start;
    }

    void go_to_address(uint32_t addr) override;
    std::vector<uint8_t> &memory_bytes() override
    {
        return memory;
    }

    // The table itself, for the white-box smoke test.
    MemoryTable *memory_table() const
    {
        return table;
    }

    uint32_t start = 0, range = 256;
    std::vector<uint8_t> memory;

  private:
    ControlRow *controls = nullptr;
    MemoryTable *table = nullptr;
};
