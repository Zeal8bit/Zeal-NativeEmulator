/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    /* Since GLSL expects the matrix to be in column-major order, we need to store A C then B D */
    AFFINE2D_REG_A  = 0,
    AFFINE2D_REG_B  = 1,
    AFFINE2D_REG_C  = 2,
    AFFINE2D_REG_D  = 3,
    AFFINE2D_REG_F  = 4,
    AFFINE2D_REG_G  = 5,
    AFFINE2D_REG_CX = 6,
    AFFINE2D_REG_CY = 7,
    AFFINE2D_REG_COUNT,
} zvb_affine2d_reg_t;

typedef struct {
    uint8_t ctrl;
    /* We have 16-bit registers representing the affine transformation matrix.
     * Store them as int to be able to pass them to the shader */
    signed int regs[AFFINE2D_REG_COUNT];
    uint8_t latch;
    bool latched;
} zvb_affine2d_t;

void zvb_affine2d_init(zvb_affine2d_t* a2d);
void zvb_affine2d_write(zvb_affine2d_t* a2d, uint32_t addr, uint8_t data);
uint8_t zvb_affine2d_read(zvb_affine2d_t* a2d, uint32_t addr);
const int* zvb_affine2d_matrix(zvb_affine2d_t* a2d);
const int* zvb_affine2d_perspective(zvb_affine2d_t* a2d);
const int* zvb_affine2d_center(zvb_affine2d_t* a2d);
