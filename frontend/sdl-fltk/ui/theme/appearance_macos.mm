/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/system_appearance.h"
#import <AppKit/AppKit.h>

namespace zui::theme {

bool system_uses_light_theme()
{
    if (@available(macOS 10.14, *)) {
        NSString* match = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:
            @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
        return [match isEqualToString:NSAppearanceNameAqua];
    }
    return true;
}

void close_system_appearance() {}

} // namespace zui::theme
