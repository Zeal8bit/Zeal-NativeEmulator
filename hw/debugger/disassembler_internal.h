/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
typedef struct {
    char*       fmt;
    char*       fmt_lab;
    uint32_t    size : 3;
    uint32_t    label : 1;
} instr_data_t;
