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
    device_t     parent;
    const char*  root_path;
    uint8_t      registers[16];
    FILE*        descriptors[MAX_OPENED_FILES];
    DIR*         directories[MAX_OPENED_FILES];
    char         names[MAX_OPENED_FILES][ZOS_MAX_NAME_LENGTH];
    const memory_op_t* host_ops;
} zeal_hostfs_t;


int hostfs_init(zeal_hostfs_t* hostfs, const memory_op_t* ops);

int hostfs_load_path(zeal_hostfs_t* hostfs, const char* root_path);
