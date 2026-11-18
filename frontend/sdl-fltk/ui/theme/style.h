/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Theme styling of stock FLTK widgets. */
#pragma once

class Fl_Button;
class Fl_Check_Button;
class Fl_Input;

namespace zui::theme {

void style_input(Fl_Input* in);
void style_button(Fl_Button* b);
void style_check(Fl_Check_Button* b);

} // namespace zui::theme
