/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "hw/zeal.h"

/* screenshot <file.ppm>: write the current ZVB frame as a binary PPM (P6). */
void console_screenshot(zeal_t* machine, int argc, char** argv);
