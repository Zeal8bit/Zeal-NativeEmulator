// SPDX-License-Identifier: Apache-2.0
// Translates FLTK key codes into the platform seam's key codes. The emulator's PS/2
// table is keyed by these, so this is the only place that knows about FLTK's
// numbering.
#include "ui/fltk/input.h"
#include "platform/display.h"
#include "platform/input.h"
#include <FL/Enumerations.H>

unsigned fltk_key_to_display(int k)
{
    // Letters, digits and the function/numpad groups are contiguous in the seam enum,
    // so those map by offset instead of one case per key.
    if (k >= 'a' && k <= 'z')
        return INPUT_KEY_A + (k - 'a');
    if (k >= 'A' && k <= 'Z')
        return INPUT_KEY_A + (k - 'A');
    if (k >= '0' && k <= '9')
        return INPUT_KEY_ZERO + (k - '0');
    if (k > FL_F && k <= FL_F + 12)
        return INPUT_KEY_F1 + (k - FL_F - 1);
    if (k >= FL_KP + '0' && k <= FL_KP + '9')
        return INPUT_KEY_KP_0 + (k - FL_KP - '0');
    switch (k) {
    case ' ': return INPUT_KEY_SPACE;
    case '\'': return INPUT_KEY_APOSTROPHE;
    case ',': return INPUT_KEY_COMMA;
    case '-': return INPUT_KEY_MINUS;
    case '.': return INPUT_KEY_PERIOD;
    case '/': return INPUT_KEY_SLASH;
    case ';': return INPUT_KEY_SEMICOLON;
    case '=': return INPUT_KEY_EQUAL;
    case '[': return INPUT_KEY_LEFT_BRACKET;
    case '\\': return INPUT_KEY_BACKSLASH;
    case ']': return INPUT_KEY_RIGHT_BRACKET;
    case '`': return INPUT_KEY_GRAVE;
    case FL_Escape: return INPUT_KEY_ESCAPE;
    case FL_Enter: return INPUT_KEY_ENTER;
    case FL_Tab: return INPUT_KEY_TAB;
    case FL_BackSpace: return INPUT_KEY_BACKSPACE;
    case FL_Insert: return INPUT_KEY_INSERT;
    case FL_Delete: return INPUT_KEY_DELETE;
    case FL_Right: return INPUT_KEY_RIGHT;
    case FL_Left: return INPUT_KEY_LEFT;
    case FL_Down: return INPUT_KEY_DOWN;
    case FL_Up: return INPUT_KEY_UP;
    case FL_Page_Up: return INPUT_KEY_PAGE_UP;
    case FL_Page_Down: return INPUT_KEY_PAGE_DOWN;
    case FL_Home: return INPUT_KEY_HOME;
    case FL_End: return INPUT_KEY_END;
    case FL_Caps_Lock: return INPUT_KEY_CAPS_LOCK;
    case FL_Scroll_Lock: return INPUT_KEY_SCROLL_LOCK;
    case FL_Num_Lock: return INPUT_KEY_NUM_LOCK;
    case FL_Print: return INPUT_KEY_PRINT_SCREEN;
    case FL_Pause: return INPUT_KEY_PAUSE;
    case FL_Shift_L: return INPUT_KEY_LEFT_SHIFT;
    case FL_Control_L: return INPUT_KEY_LEFT_CONTROL;
    case FL_Alt_L: return INPUT_KEY_LEFT_ALT;
    case FL_Meta_L: return INPUT_KEY_LEFT_SUPER;
    case FL_Shift_R: return INPUT_KEY_RIGHT_SHIFT;
    case FL_Control_R: return INPUT_KEY_RIGHT_CONTROL;
    case FL_Alt_R: return INPUT_KEY_RIGHT_ALT;
    case FL_Meta_R: return INPUT_KEY_RIGHT_SUPER;
    case FL_Menu: return INPUT_KEY_KB_MENU;
    case FL_KP_Enter: return INPUT_KEY_KP_ENTER;
    case FL_KP + '.': return INPUT_KEY_KP_DECIMAL;
    case FL_KP + '/': return INPUT_KEY_KP_DIVIDE;
    case FL_KP + '*': return INPUT_KEY_KP_MULTIPLY;
    case FL_KP + '-': return INPUT_KEY_KP_SUBTRACT;
    case FL_KP + '+': return INPUT_KEY_KP_ADD;
    default: return INPUT_KEY_NONE;
    }
}
