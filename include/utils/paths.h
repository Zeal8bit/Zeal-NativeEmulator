/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <string.h>

#ifdef _WIN32
    /* Including windows.h can lead to conflicts with Raylib API, stdlib defines PATH_MAX */
    #include <stdlib.h>
    #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
    #define HOME_VAR      "APPDATA"
    #define HOME_SANITIZE "%%APPDATA%%"
#elif __linux__
    #include <unistd.h>
    #include <linux/limits.h>
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
#elif PLATFORM_WEB
    #define PATH_MAX 4096
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
#endif


#ifdef _WIN32
static inline int os_mkdir(const char* path, int mode) {
    (void) mode;
    extern int mkdir(const char*);
    return mkdir(path);
}

#else

#define os_mkdir mkdir

#endif // _WIN32

void get_executable_path(char *buffer, size_t size);
void get_executable_dir(char *buffer, size_t size);

int get_install_dir_file(char dst[PATH_MAX], const char* name);
const char* get_shaders_path(char dst[PATH_MAX], const char* name);

int path_exists(const char *path);
char* get_relative_path(const char* absolute_path);
const char* get_home_dir();
const char* get_config_dir();
const char* get_config_path();
const char* path_sanitize(const char* path);
