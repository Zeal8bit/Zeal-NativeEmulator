/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "utils/paths.h"
#include "utils/config.h"
#include "utils/log.h"

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
    snprintf(path, sizeof(path), "%s/.zeal8bit", home);

    if(os_mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return NULL;
    }

    return path;
}

const char *get_config_path(void) {
    static char path[PATH_MAX];
    const char* config_dir = get_config_dir();

    if(os_mkdir(config_dir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/" CONFIG_USER_FILENAME, config_dir);
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


#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

FILE* path_create_temp(const char* target, char* tmp, size_t tmp_size)
{
    for (unsigned attempt = 0; attempt < 100; attempt++) {
        snprintf(tmp, tmp_size, "%s.%lu-%u.tmp", target, (unsigned long) GetCurrentProcessId(), attempt);
        const int fd = _open(tmp, _O_CREAT | _O_EXCL | _O_WRONLY | _O_TEXT, _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            FILE* f = _fdopen(fd, "w");
            if (f == NULL) {
                _close(fd);
                remove(tmp);
            }
            return f;
        }
        if (errno != EEXIST) return NULL;
    }
    return NULL;
}

int path_commit(FILE* file)
{
    if (fflush(file) != 0) return -1;
    return _commit(_fileno(file));
}

int path_replace(const char* tmp, const char* target)
{
    if (MoveFileExA(tmp, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
    errno = EACCES;
    return -1;
}

#else

FILE* path_create_temp(const char* target, char* tmp, size_t tmp_size)
{
    if ((size_t) snprintf(tmp, tmp_size, "%s.XXXXXX", target) >= tmp_size) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    const int fd = mkstemp(tmp);
    if (fd < 0) return NULL;
    /* Keep the permissions of the file being replaced; a new file gets the
     * usual ones (mkstemp creates it private) */
    struct stat st;
    mode_t mode;
    if (stat(target, &st) == 0) {
        mode = st.st_mode & 07777;
    } else {
        const mode_t mask = umask(0);
        umask(mask);
        mode = 0666 & ~mask;
    }
    FILE* f = fchmod(fd, mode) == 0 ? fdopen(fd, "w") : NULL;
    if (f == NULL) {
        const int err = errno;
        close(fd);
        unlink(tmp);
        errno = err;
    }
    return f;
}

int path_commit(FILE* file)
{
    if (fflush(file) != 0) return -1;
    return fsync(fileno(file));
}

int path_replace(const char* tmp, const char* target)
{
    return rename(tmp, target);
}

#endif
