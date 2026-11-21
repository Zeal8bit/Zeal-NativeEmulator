/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/system_appearance.h"
#include <windows.h>

namespace zui::theme {

bool system_uses_light_theme()
{
    DWORD value;
    DWORD size = sizeof(value);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    /* Windows defaults to light when no app preference has been saved. */
    return result == ERROR_FILE_NOT_FOUND || (result == ERROR_SUCCESS && value != 0);
}

void close_system_appearance() {}

} // namespace zui::theme
