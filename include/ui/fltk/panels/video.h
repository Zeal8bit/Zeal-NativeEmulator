// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/fltk/panel.h"
#include "ui/fltk/widgets/canvas.h"

// The live screen. Unlike the VRAM views this one fits the frame into the canvas and,
// when focused, turns pointer and keyboard events into guest input.
class VideoCanvas : public ImageCanvas
{
  public:
    VideoCanvas(dbg_ui_t *u, int x, int y, int w, int h);
    // True while this canvas owns the FLTK grab for relative mouse input.
    bool captured = false;
    // Drops the capture and forgets the sub-pixel mouse remainder.
    void release_capture();

  protected:
    std::string footer() const override
    {
        return "Click video for keyboard";
    }

    void on_wheel(int delta, bool horizontal) override;
    void on_click() override;
    void on_pointer(int event) override;
    bool on_key(int event) override;
    void on_focus_lost() override;

  private:
    int mouse_x = 0, mouse_y = 0;
    double fraction_x = 0, fraction_y = 0;
};

class VideoPanel : public Panel
{
  public:
    explicit VideoPanel(dbg_ui_t *u);
    void refresh() override;
    bool captures_input() const override
    {
        return canvas->captured;
    }

    int forward_input(int event) override
    {
        return canvas->handle(event);
    }

    void release_capture() override
    {
        canvas->release_capture();
    }

    bool input_focused() const override;
    VideoCanvas *canvas = nullptr;
};
