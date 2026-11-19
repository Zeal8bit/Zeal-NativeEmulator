/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "frontend/sdl-fltk/paths.h"
#include "utils/paths.h"
#include <libgen.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "utils/config.h"

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

const char *get_config_path(void) {
    static char path[PATH_MAX];
    const char* config_dir = get_config_dir();

    if (!config_dir) return NULL;
    if (os_mkdir(config_dir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return NULL;
    }

    if (snprintf(path, sizeof(path), "%s/" CONFIG_USER_FILENAME, config_dir) >= (int)sizeof(path)) return NULL;
    return path;
}

int path_same_file(const char* a, const char* b)
{
    struct stat sa, sb;
    char ra[PATH_MAX];
    char rb[PATH_MAX];
    if (a == NULL || b == NULL) return 0;
    if (stat(a, &sa) == 0 && stat(b, &sb) == 0 && sa.st_ino != 0 && sb.st_ino != 0) {
        return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
    }
    if (realpath(a, ra) == NULL || realpath(b, rb) == NULL) return strcmp(a, b) == 0;
    return strcmp(ra, rb) == 0;
}
