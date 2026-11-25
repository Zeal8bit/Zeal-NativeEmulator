/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "hw/zeal.h"

bool web_debug_requested(void);
void web_debug_start(zeal_t* machine);
bool web_debug_service(zeal_t* machine);
void web_debug_after_step(zeal_t* machine);
