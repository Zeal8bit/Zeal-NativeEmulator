/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "frontend/sdl-fltk/paths.h"
#include "utils/paths.h"
#include <libgen.h>

#ifdef _WIN32
#include <windows.h>
#endif

const char* fltk_executable_dir(void)
{
    static char path[PATH_MAX];
#ifdef _WIN32
    DWORD size = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!size || size >= sizeof(path)) return NULL;
#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return NULL;
#elif defined(__linux__)
    ssize_t size = readlink("/proc/self/exe", path, sizeof(path));
    if (size < 0 || (size_t)size >= sizeof(path)) return NULL;
    path[size] = '\0';
#else
    return NULL;
#endif
    return dirname(path);
}
