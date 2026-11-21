/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app/keymap.h"
#include <FL/Enumerations.H>

extern "C" {
#include "hw/keys.h"
}

namespace zui::app {

/* macOS derives Fl::event_key() from charactersIgnoringModifiers, which keeps
 * Shift for digits/punctuation (Shift+; reports ':'). Map shifted characters
 * back to the physical key on a US layout; Shift itself is sent separately. */
static int unshift_us(int key)
{
    static const char shifted[]   = ":\"<>?_+{}|~!@#$%^&*()";
    static const char unshifted[] = ";',./-=[]\\`1234567890";
    if (key >= 'A' && key <= 'Z') return key - 'A' + 'a';
    for (int i = 0; shifted[i]; i++) {
        if (key == (unsigned char) shifted[i]) return (unsigned char) unshifted[i];
    }
    return key;
}

int fltk_to_zeal_key(int key)
{
    key = unshift_us(key);
    /* Printable keys: the unshifted ASCII character */
    if (key >= 'a' && key <= 'z') return ZEAL_KEY_A + (key - 'a');
    if (key >= '0' && key <= '9') return ZEAL_KEY_ZERO + (key - '0');
    switch (key) {
        case '\'': return ZEAL_KEY_APOSTROPHE;
        case ',':  return ZEAL_KEY_COMMA;
        case '-':  return ZEAL_KEY_MINUS;
        case '.':  return ZEAL_KEY_PERIOD;
        case '/':  return ZEAL_KEY_SLASH;
        case ';':  return ZEAL_KEY_SEMICOLON;
        case '=':  return ZEAL_KEY_EQUAL;
        case '[':  return ZEAL_KEY_LEFT_BRACKET;
        case '\\': return ZEAL_KEY_BACKSLASH;
        case ']':  return ZEAL_KEY_RIGHT_BRACKET;
        case '`':  return ZEAL_KEY_GRAVE;
        case ' ':  return ZEAL_KEY_SPACE;
        case FL_Escape:      return ZEAL_KEY_ESCAPE;
        case FL_Enter:       return ZEAL_KEY_ENTER;
        case FL_Tab:         return ZEAL_KEY_TAB;
        case FL_BackSpace:   return ZEAL_KEY_BACKSPACE;
        case FL_Insert:      return ZEAL_KEY_INSERT;
        case FL_Help:        return ZEAL_KEY_INSERT; /* Mac "Help" key position */
        case FL_Delete:      return ZEAL_KEY_DELETE;
        case FL_Right:       return ZEAL_KEY_RIGHT;
        case FL_Left:        return ZEAL_KEY_LEFT;
        case FL_Down:        return ZEAL_KEY_DOWN;
        case FL_Up:          return ZEAL_KEY_UP;
        case FL_Page_Up:     return ZEAL_KEY_PAGE_UP;
        case FL_Page_Down:   return ZEAL_KEY_PAGE_DOWN;
        case FL_Home:        return ZEAL_KEY_HOME;
        case FL_End:         return ZEAL_KEY_END;
        case FL_Caps_Lock:   return ZEAL_KEY_CAPS_LOCK;
        case FL_Scroll_Lock: return ZEAL_KEY_SCROLL_LOCK;
        case FL_Num_Lock:    return ZEAL_KEY_NUM_LOCK;
        case FL_Print:       return ZEAL_KEY_PRINT_SCREEN;
        case FL_Pause:       return ZEAL_KEY_PAUSE;
        case FL_Shift_L:     return ZEAL_KEY_LEFT_SHIFT;
        case FL_Control_L:   return ZEAL_KEY_LEFT_CONTROL;
        case FL_Alt_L:       return ZEAL_KEY_LEFT_ALT;
        case FL_Meta_L:      return ZEAL_KEY_LEFT_SUPER;
        case FL_Shift_R:     return ZEAL_KEY_RIGHT_SHIFT;
        case FL_Control_R:   return ZEAL_KEY_RIGHT_CONTROL;
        case FL_Alt_R:       return ZEAL_KEY_RIGHT_ALT;
        case FL_Meta_R:      return ZEAL_KEY_RIGHT_SUPER;
        case FL_Menu:        return ZEAL_KEY_KB_MENU;
        case FL_KP_Enter:    return ZEAL_KEY_KP_ENTER;
        case FL_KP + '.':    return ZEAL_KEY_KP_DECIMAL;
        case FL_KP + '/':    return ZEAL_KEY_KP_DIVIDE;
        case FL_KP + '*':    return ZEAL_KEY_KP_MULTIPLY;
        case FL_KP + '-':    return ZEAL_KEY_KP_SUBTRACT;
        case FL_KP + '+':    return ZEAL_KEY_KP_ADD;
        case FL_KP + '=':    return ZEAL_KEY_KP_EQUAL;
        default: break;
    }
    if (key >= FL_KP + '0' && key <= FL_KP + '9') return ZEAL_KEY_KP_0 + (key - FL_KP - '0');
    if (key >= FL_F + 1 && key <= FL_F + 12) return ZEAL_KEY_F1 + (key - FL_F - 1);
    return ZEAL_KEY_NULL;
}

} // namespace zui::app
