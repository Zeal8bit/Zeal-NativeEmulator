/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "hw/hostfs.h"

void frontend_hostfs_bind(zeal_hostfs_t* core);
void frontend_hostfs_close(void);
