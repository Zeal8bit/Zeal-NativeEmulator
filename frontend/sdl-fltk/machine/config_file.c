/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frontend/sdl-fltk/config.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils/paths.h"
#include "utils/log.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

static FILE* path_create_temp(const char* target, char* tmp, size_t tmp_size)
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

static int path_commit(FILE* file)
{
    if (fflush(file) != 0) return -1;
    return _commit(_fileno(file));
}

static int path_replace(const char* tmp, const char* target)
{
    if (MoveFileExA(tmp, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
    errno = EACCES;
    return -1;
}

#else

static FILE* path_create_temp(const char* target, char* tmp, size_t tmp_size)
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

static int path_commit(FILE* file)
{
    if (fflush(file) != 0) return -1;
    return fsync(fileno(file));
}

static int path_replace(const char* tmp, const char* target)
{
    return rename(tmp, target);
}

#endif

static const char* module = "[CONFIG]";

bool config_write_file(const char* path, bool (*write_contents)(FILE*, const void*), const void* data)
{
    char target[PATH_MAX];
    char tmp[PATH_MAX + 32];
    struct stat st;
    FILE* f = NULL;
    bool ok = true;

    if (realpath(path, target) == NULL) {
        if (errno != ENOENT || strlen(path) >= sizeof(target)) goto fail;
#ifndef _WIN32
        /* A symbolic link to a missing file: replacing the link itself
         * would silently detach it */
        if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
            log_err_printf("%s %s is a link to a missing file: not saved\n", module, path_sanitize(path));
            return false;
        }
#endif
        strcpy(target, path);   /* new file */
    }
    if (stat(target, &st) == 0) {
        /* Only an ordinary file the user may write is replaced: not a
         * read-only file, a FIFO or a device, even in a writable folder,
         * and not a file with several names (hard links), which cannot all
         * be replaced at once */
        const char* why = !S_ISREG(st.st_mode) ? "is not a regular file"
                        : access(target, W_OK) != 0 ? "is not writable"
                        : st.st_nlink > 1 ? "has several hard links" : NULL;
        if (why) {
            log_err_printf("%s %s %s: not saved, the file is unchanged\n", module, path_sanitize(path), why);
            return false;
        }
    }
    f = path_create_temp(target, tmp, sizeof(tmp));
    if (f == NULL) goto fail;
    ok = write_contents(f, data);
    if (ok) ok = path_commit(f) == 0;
    {
        const int err = errno;
        if (fclose(f) != 0) ok = false;
        else errno = err;
    }
    if (ok && path_replace(tmp, target) == 0) return true;
    {
        const int err = errno;
        remove(tmp);
        errno = err;
    }
fail:
    log_err_printf("%s Could not save %s: %s\n", module, path_sanitize(path), strerror(errno));
    return false;
}
