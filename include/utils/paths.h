/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    /* Including windows.h can lead to conflicts with Raylib API, stdlib defines PATH_MAX */
    #include <stdlib.h>
    #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
    #define HOME_VAR      "APPDATA"
    #define HOME_SANITIZE "%%APPDATA%%"
    #define FOPEN_BINARY  "b"
    #define OPEN_BINARY   O_BINARY
#elif __linux__
    #include <unistd.h>
    #include <linux/limits.h>
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
    #define FOPEN_BINARY  ""
    #define OPEN_BINARY   0
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
    #define FOPEN_BINARY  ""
    #define OPEN_BINARY   0
#else
    #include <limits.h>
    #ifndef PATH_MAX
    #define PATH_MAX 4096
    #endif
    #define HOME_VAR      "HOME"
    #define HOME_SANITIZE "~"
    #define FOPEN_BINARY  ""
    #define OPEN_BINARY   0
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

int path_exists(const char *path);
char* get_relative_path(const char* absolute_path);
const char* get_home_dir();
const char* get_config_dir();
const char* get_config_path();
const char* path_sanitize(const char* path);

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Non-zero when both paths name the same existing file: relative and
 * absolute spellings, symbolic links and hard links. Uses the file identity
 * (device, inode) when the platform provides one, else (e.g. Windows, where
 * st_ino is 0) compares canonical paths. Paths that do not exist compare
 * by their canonical or literal spelling.
 */
int path_same_file(const char* a, const char* b);

/**
 * @brief Safe replacement of a file: create a new, exclusive temporary file
 * next to `target` (its name goes to `tmp`), write it, path_commit() it,
 * then path_replace() moves it over `target` in one step. The temporary
 * file takes `target`'s permissions when it exists (POSIX). Each returns
 * NULL / non-zero on failure with errno set; `target` is untouched until
 * path_replace() succeeds.
 */
FILE* path_create_temp(const char* target, char* tmp, size_t tmp_size);
int path_commit(FILE* file);
int path_replace(const char* tmp, const char* target);
#ifdef __cplusplus
}
#endif
