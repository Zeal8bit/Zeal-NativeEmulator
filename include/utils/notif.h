/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

void notif_reset(void);
void notif_show(const char* fmt, ...);
int notif_estimate_width(void);
void notif_render(int x, int y);
bool notif_visible(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
