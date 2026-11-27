/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "hw/zeal.h"
zeal_step_result_t wasm_frame(zeal_t* machine);
bool wasm_present(zeal_t* machine);
