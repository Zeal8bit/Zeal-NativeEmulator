/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdio.h>
#include <dirent.h>
#include "hw/hostfs.h"

#define MAX_OPENED_FILES        256
#define ZOS_MAX_NAME_LENGTH     16


typedef struct {
    uint8_t is_dir;
    char    name[ZOS_MAX_NAME_LENGTH];
#ifdef _WIN32
    /* On Windows, we cannot stat an opened directory, so keep the path */
    char    path[256];
#endif
    union {
        FILE* file;
        DIR*  dir;
        void* raw;
    };
} hostfs_fd_t;

typedef struct {
    zeal_hostfs_t* core;
    const char* root_path;
    hostfs_fd_t descriptors[MAX_OPENED_FILES];
} frontend_hostfs_t;
extern frontend_hostfs_t frontend_hostfs;
void frontend_hostfs_bind(frontend_hostfs_t*, zeal_hostfs_t*);
void                     frontend_hostfs_close(frontend_hostfs_t*);
int frontend_hostfs_load_path(frontend_hostfs_t*, const char* path);
