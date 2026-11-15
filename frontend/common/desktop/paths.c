/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "utils/paths.h"

#ifdef _WIN32
    #include <libloaderapi.h>
    void get_executable_path(char *buffer, size_t size) {
        GetModuleFileName(NULL, buffer, (DWORD)size);
    }
#elif __linux__
    #include <unistd.h>
    #include <limits.h>
    void get_executable_path(char *buffer, size_t size) {
        ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
        if (len != -1) buffer[len] = '\0';
    }
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
    void get_executable_path(char *buffer, size_t size) {
        uint32_t bufsize = (uint32_t)size;
        if (_NSGetExecutablePath(buffer, &bufsize) != 0) {
            fprintf(stderr, "Buffer too small; need size %u\n", bufsize);
        }
    }
#else
    #error "Unsupported platform"
#endif
