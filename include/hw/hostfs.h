/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include "hw/device.h"
#include "hw/memory_op.h"


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
    device_t     parent;
    const char*  root_path;
    uint8_t      registers[16];
    hostfs_fd_t  descriptors[MAX_OPENED_FILES];
    const memory_op_t* host_ops;
} zeal_hostfs_t;


int hostfs_init(zeal_hostfs_t* hostfs, const memory_op_t* ops);

int hostfs_load_path(zeal_hostfs_t* hostfs, const char* root_path);

#ifdef PLATFORM_WEB
void hostfs_web_complete(uint8_t status, uint8_t reg0, uint8_t reg1,
                         uint8_t reg2, uint8_t reg3, uint8_t reg4, uint8_t reg5);
void hostfs_web_write_guest(uint16_t address, uint8_t* data, uint16_t length);
#endif
