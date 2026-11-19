/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "utils/paths.h"

int path_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

const char* get_home_dir(void) {
    const char* home = getenv(HOME_VAR);
    if (!home) {
        fprintf(stderr, HOME_VAR " environment variable not set\n");
        return NULL;
    }
    return home;
}


const char *get_config_dir(void) {
    static char path[PATH_MAX];
    const char* home = get_home_dir();
    if (!home || snprintf(path, sizeof(path), "%s/.zeal8bit", home) >= (int)sizeof(path)) return NULL;
    return path;
}

const char* path_sanitize(const char* path) {
    static char sanitized[PATH_MAX];
    const char* home = get_home_dir();
    size_t home_len = home ? strlen(home) : 0;

    if (home && strncmp(path, home, home_len) == 0) {
        snprintf(sanitized, sizeof(sanitized), HOME_SANITIZE "/%s", path + home_len + 1);
    } else {
        snprintf(sanitized, sizeof(sanitized), "%s", path);
    }

    return sanitized;
}
