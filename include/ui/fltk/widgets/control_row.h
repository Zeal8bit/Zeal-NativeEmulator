// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <FL/Fl_Flex.H>

class Fl_Choice;
class TextInput;

// The row of controls a panel puts above its table or canvas: an optional address or
// symbol field and an optional view/range chooser. It keeps the input taking the slack
// and the chooser sized to its widest entry, so no panel needs magic pixel widths.
class ControlRow : public Fl_Flex
{
  public:
    explicit ControlRow(int row_height);

    // Adds the address/symbol field. Returns it so the panel can read its value.
    TextInput *add_location(const char *initial, const char *tooltip);
    // Adds the chooser with a '|' separated list. Returns it for reading the value.
    Fl_Choice *add_choice(const char *items);
    // Re-measures the chooser against the current font. Call from resize().
    void layout(int row_height, int pad, int font, int font_size);

    TextInput *location() const
    {
        return location_;
    }

    Fl_Choice *choice() const
    {
        return choice_;
    }

  private:
    TextInput *location_ = nullptr;
    Fl_Choice *choice_ = nullptr;
};
