/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <vector>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>

namespace zui::memtools {

class FormDialog : public Fl_Double_Window {
public:
    static constexpr int W = 400;
    static constexpr int ROW_H = 30;
    static constexpr int LABEL_W = 92;

    FormDialog(const char* title, const char* ok_label, const std::vector<const char*>& labels,
               const char* note);
    Fl_Input* input(int i) const;
    const char* value(int i) const;
    void value(int i, const char* text);
    void note(const char* text);
    void error(const char* text);
    void warn(const char* text);

    /** Show modally until OK (accepted by `accept`) or Cancel/Escape. */
    bool run(const std::function<bool(FormDialog&)>& accept);
    void draw() override;

private:
    void message(const char* text, Fl_Color c);
    static void cb_ok(Fl_Widget*, void* data);
    static void cb_cancel(Fl_Widget*, void* data);

    const char* m_title;
    std::vector<Fl_Input*> m_inputs;
    Fl_Box* m_note;
    Fl_Box* m_message;
    const std::function<bool(FormDialog&)>* m_accept = nullptr;
    bool m_result = false;
};

} // namespace zui::memtools
