// SPDX-License-Identifier: Apache-2.0
#include "input.h"
#include <FL/Enumerations.H>
unsigned fltk_key_to_host(int k)
{
    if (k >= 'a' && k <= 'z')
        return k - 'a' + 'A';
    if (k >= 32 && k <= 96)
        return k;
    if (k > FL_F && k <= FL_F + 12)
        return 289 + k - FL_F;
    if (k >= FL_KP + '0' && k <= FL_KP + '9')
        return 320 + k - FL_KP - '0';
    switch (k) {
    case FL_Escape:
        return 256;
    case FL_Enter:
        return 257;
    case FL_Tab:
        return 258;
    case FL_BackSpace:
        return 259;
    case FL_Insert:
        return 260;
    case FL_Delete:
        return 261;
    case FL_Right:
        return 262;
    case FL_Left:
        return 263;
    case FL_Down:
        return 264;
    case FL_Up:
        return 265;
    case FL_Page_Up:
        return 266;
    case FL_Page_Down:
        return 267;
    case FL_Home:
        return 268;
    case FL_End:
        return 269;
    case FL_Caps_Lock:
        return 280;
    case FL_Scroll_Lock:
        return 281;
    case FL_Num_Lock:
        return 282;
    case FL_Print:
        return 283;
    case FL_Pause:
        return 284;
    case FL_Shift_L:
        return 340;
    case FL_Control_L:
        return 341;
    case FL_Alt_L:
        return 342;
    case FL_Meta_L:
        return 343;
    case FL_Shift_R:
        return 344;
    case FL_Control_R:
        return 345;
    case FL_Alt_R:
        return 346;
    case FL_Meta_R:
        return 347;
    case FL_KP_Enter:
        return 335;
    case FL_KP + '.':
        return 330;
    case FL_KP + '/':
        return 331;
    case FL_KP + '*':
        return 332;
    case FL_KP + '-':
        return 333;
    case FL_KP + '+':
        return 334;
    default:
        return 0;
    }
}
