/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <string.h>

#ifdef _WIN32
    /* Including windows.h can lead to conflicts with Raylib API */
    #define PATH_MAX    256
    #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#elif __linux__
    #include <unistd.h>
    #include <linux/limits.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
#elif PLATFORM_WEB
    #define PATH_MAX 4096
#endif


void get_executable_path(char *buffer, size_t size);
void get_executable_dir(char *buffer, size_t size);

int get_install_dir_file(char dst[PATH_MAX], const char* name);
const char* get_shaders_path(char dst[PATH_MAX], const char* name);

int path_exists(const char *path);
char* get_relative_path(const char* absolute_path);
