/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils/paths.h"
#include "utils/log.h"

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
