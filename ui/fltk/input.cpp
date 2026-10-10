// SPDX-License-Identifier: Apache-2.0
// Translates FLTK key codes into the host seam's key codes. The emulator's PS/2
// table is keyed by these, so this is the only place that knows about FLTK's
// numbering.
#include "ui/fltk/input.h"
#include "host/zeal_host.h"
#include <FL/Enumerations.H>

unsigned fltk_key_to_host(int k)
{
    // Letters, digits and the function/numpad groups are contiguous in the seam enum,
    // so those map by offset instead of one case per key.
    if (k >= 'a' && k <= 'z')
        return ZEAL_HOST_KEY_A + (k - 'a');
    if (k >= 'A' && k <= 'Z')
        return ZEAL_HOST_KEY_A + (k - 'A');
    if (k >= '0' && k <= '9')
        return ZEAL_HOST_KEY_ZERO + (k - '0');
    if (k > FL_F && k <= FL_F + 12)
        return ZEAL_HOST_KEY_F1 + (k - FL_F - 1);
    if (k >= FL_KP + '0' && k <= FL_KP + '9')
        return ZEAL_HOST_KEY_KP_0 + (k - FL_KP - '0');
    switch (k) {
    case ' ': return ZEAL_HOST_KEY_SPACE;
    case '\'': return ZEAL_HOST_KEY_APOSTROPHE;
    case ',': return ZEAL_HOST_KEY_COMMA;
    case '-': return ZEAL_HOST_KEY_MINUS;
    case '.': return ZEAL_HOST_KEY_PERIOD;
    case '/': return ZEAL_HOST_KEY_SLASH;
    case ';': return ZEAL_HOST_KEY_SEMICOLON;
    case '=': return ZEAL_HOST_KEY_EQUAL;
    case '[': return ZEAL_HOST_KEY_LEFT_BRACKET;
    case '\\': return ZEAL_HOST_KEY_BACKSLASH;
    case ']': return ZEAL_HOST_KEY_RIGHT_BRACKET;
    case '`': return ZEAL_HOST_KEY_GRAVE;
    case FL_Escape: return ZEAL_HOST_KEY_ESCAPE;
    case FL_Enter: return ZEAL_HOST_KEY_ENTER;
    case FL_Tab: return ZEAL_HOST_KEY_TAB;
    case FL_BackSpace: return ZEAL_HOST_KEY_BACKSPACE;
    case FL_Insert: return ZEAL_HOST_KEY_INSERT;
    case FL_Delete: return ZEAL_HOST_KEY_DELETE;
    case FL_Right: return ZEAL_HOST_KEY_RIGHT;
    case FL_Left: return ZEAL_HOST_KEY_LEFT;
    case FL_Down: return ZEAL_HOST_KEY_DOWN;
    case FL_Up: return ZEAL_HOST_KEY_UP;
    case FL_Page_Up: return ZEAL_HOST_KEY_PAGE_UP;
    case FL_Page_Down: return ZEAL_HOST_KEY_PAGE_DOWN;
    case FL_Home: return ZEAL_HOST_KEY_HOME;
    case FL_End: return ZEAL_HOST_KEY_END;
    case FL_Caps_Lock: return ZEAL_HOST_KEY_CAPS_LOCK;
    case FL_Scroll_Lock: return ZEAL_HOST_KEY_SCROLL_LOCK;
    case FL_Num_Lock: return ZEAL_HOST_KEY_NUM_LOCK;
    case FL_Print: return ZEAL_HOST_KEY_PRINT_SCREEN;
    case FL_Pause: return ZEAL_HOST_KEY_PAUSE;
    case FL_Shift_L: return ZEAL_HOST_KEY_LEFT_SHIFT;
    case FL_Control_L: return ZEAL_HOST_KEY_LEFT_CONTROL;
    case FL_Alt_L: return ZEAL_HOST_KEY_LEFT_ALT;
    case FL_Meta_L: return ZEAL_HOST_KEY_LEFT_SUPER;
    case FL_Shift_R: return ZEAL_HOST_KEY_RIGHT_SHIFT;
    case FL_Control_R: return ZEAL_HOST_KEY_RIGHT_CONTROL;
    case FL_Alt_R: return ZEAL_HOST_KEY_RIGHT_ALT;
    case FL_Meta_R: return ZEAL_HOST_KEY_RIGHT_SUPER;
    case FL_Menu: return ZEAL_HOST_KEY_KB_MENU;
    case FL_KP_Enter: return ZEAL_HOST_KEY_KP_ENTER;
    case FL_KP + '.': return ZEAL_HOST_KEY_KP_DECIMAL;
    case FL_KP + '/': return ZEAL_HOST_KEY_KP_DIVIDE;
    case FL_KP + '*': return ZEAL_HOST_KEY_KP_MULTIPLY;
    case FL_KP + '-': return ZEAL_HOST_KEY_KP_SUBTRACT;
    case FL_KP + '+': return ZEAL_HOST_KEY_KP_ADD;
    default: return ZEAL_HOST_KEY_NONE;
    }
}
