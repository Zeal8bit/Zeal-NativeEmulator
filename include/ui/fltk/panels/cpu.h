// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/canvas.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Grid.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <array>

// The CPU panel's condition-flags line.
class FlagsBar : public Fl_Widget
{
  public:
    FlagsBar(dbg_ui_t *u, int x, int y, int w, int h) : Fl_Widget(x, y, w, h), ui(u)
    {
    }

    dbg_ui_t *ui;
    uint8_t flags = 0;

    void draw() override;
};

// Register pairs you can type into, plus the condition-flags line.
class CpuPanel : public Panel
{
  public:
    explicit CpuPanel(dbg_ui_t *u);
    void refresh() override;
    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;

    std::array<Fl_Box *, 14> register_labels{};
    std::array<Fl_Input *, 14> registers{};
    FlagsBar *flags = nullptr;
    regs_t regs{};

  private:
    Fl_Grid *grid = nullptr;
    void apply_register(Fl_Widget *field);
};
