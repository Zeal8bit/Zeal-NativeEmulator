// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl_Table.H>
#include <cstdint>
#include <vector>

struct dbg_ui_t;

// The buffer a MemoryTable shows and edits. The Memory panel implements it, so the
// table does not depend on the panel and the panel keeps owning its address and range.
class MemorySource
{
  public:
    virtual ~MemorySource() = default;
    virtual uint32_t memory_start() const = 0;
    virtual std::vector<uint8_t> &memory_bytes() = 0;
};

// Memory uses a real FLTK table instead of a hand-painted canvas so it gets the
// scheme-drawn scrollbars, cell selection, keyboard navigation and column headers.
// Fl_Table_Row is deliberately avoided: it forces whole-row selection, which
// conflicts with the byte-granular cell selection a hex view wants.
class MemoryTable : public Fl_Table
{
  public:
    MemorySource *source;
    dbg_ui_t *ui;
    int hover = -1, cursor = -1, anchor = -1, nibble = 0, edit_col = 1;
    int sel_lo = -1, sel_hi = -1;
    int shown_columns = 0, row_metric = 0, row_count = 0;
    int address_width = 0, hex_width = 0, ascii_width = 0;
    // View state used to skip repaints that would otherwise flicker at refresh rate.
    uint32_t shown_start = ~0u;
    bool shown_upper = true, shown_cp437 = false;
    std::vector<uint8_t> shown;

    MemoryTable(MemorySource *source, dbg_ui_t *ui, int x, int y, int w, int h);
    int cell_width() const;
    int byte_columns() const;
    int byte_at_event(int *col_out = nullptr, int *row_out = nullptr);
    bool sync(size_t bytes);
    bool refresh_view(size_t bytes, uint32_t start, bool upper, bool cp437);
    void reset_scroll();
    void update_selection();
    void set_cursor(int index, int column, bool extend = false);
    bool key_event();
    bool write_byte(int index, uint8_t value);
    void update_hover();
    void draw_cell(TableContext context, int R = 0, int C = 0, int X = 0, int Y = 0, int W = 0,
                   int H = 0) override;
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;
};
