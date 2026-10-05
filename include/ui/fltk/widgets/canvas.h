// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "debugger/frontend.h"
#include <FL/Fl_Widget.H>
#include <cstdint>
#include <string>
#include <vector>

struct dbg_ui_t;

// Draws an emulated frame with explicit nearest-neighbour scaling. Subclasses supply the
// panel-specific behaviour: Video can grab the mouse and feed guest input, VRAM tracks
// the cell under the pointer and draws a magnified preview.
class ImageCanvas : public Fl_Widget
{
  public:
    // FIT centres the whole frame in the canvas; SCALED zooms by the shell scale and
    // scrolls, which is what the VRAM views want.
    enum Placement { FIT, SCALED };

    std::vector<uint8_t> pixels;
    dbg_image_info_t image{};
    int scroll_x = 0, scroll_y = 0;
    int hover = -1;

    ImageCanvas(dbg_ui_t *u, int x, int y, int w, int h, Placement placement);
    // Re-reads the frame for `view` (-1 is the live screen). Returns false when no frame
    // is available yet.
    bool fetch(int view);
    void draw() override;
    int handle(int event) override;

  protected:
    dbg_ui_t *ui;
    Placement placement;
    // Hint drawn under the image.
    virtual std::string footer() const
    {
        return "Wheel scroll  Shift horizontal";
    }

    // Decoration drawn over the image, in canvas coordinates.
    virtual void decorate(int, int)
    {
    }

    // Pointer moved over the image; px/py are in image pixels and may be outside it.
    virtual void hover_at(int, int)
    {
    }

    virtual void on_wheel(int delta, bool horizontal);
    virtual void on_click()
    {
    }

    virtual void on_pointer(int)
    {
    }

    virtual bool on_key(int)
    {
        return false;
    }

    virtual void on_focus_lost()
    {
    }

    // Maximum scroll offset for the current frame and canvas size.
    int maximum_scroll(bool horizontal) const;
};
