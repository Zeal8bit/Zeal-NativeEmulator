// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/canvas.h"
#include "ui/fltk/widgets/control_row.h"

// One of the five VRAM views, zoomed and scrollable, with a magnified cell preview and
// a decoded cell readout under the pointer.
class VramCanvas : public ImageCanvas
{
  public:
    class VramPanel *panel;
    VramCanvas(class VramPanel *p, dbg_ui_t *u, int x, int y, int w, int h);

  protected:
    std::string footer() const override;
    void decorate(int row, int pad) override;
    void hover_at(int px, int py) override;
};

class VramPanel : public Panel
{
  public:
    explicit VramPanel(dbg_ui_t *u);
    void refresh() override;
    void resize(int x, int y, int w, int h) override;
    int selected_view() const override;

    // Selects one of the five views, as the View menu would.
    void select_view(int view);

    dbg_vram_info_t vram{};
    VramCanvas *canvas = nullptr;

  private:
    ControlRow *controls = nullptr;
};
