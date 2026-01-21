/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include "utils/log.h"
#include "hw/zvb/zvb_affine2d.h"

#define AFFINE2D_REG_CTRL 0
#define AFFINE2D_REG_CTRL_ENABLE (1 << 7)
#define AFFINE2D_REG_CTRL_WRAP   (1 << 6)

#define AFFINE2D_ADDR_REG_A     1
#define AFFINE2D_ADDR_REG_B     2
#define AFFINE2D_ADDR_REG_C     3
#define AFFINE2D_ADDR_REG_D     4
#define AFFINE2D_ADDR_REG_F     5
#define AFFINE2D_ADDR_REG_G     6
#define AFFINE2D_ADDR_REG_CX    7
#define AFFINE2D_ADDR_REG_CY    8

void zvb_affine2d_init(zvb_affine2d_t* a2d)
{
    a2d->ctrl = AFFINE2D_REG_CTRL_ENABLE;
    for (int i = 0; i < AFFINE2D_REG_COUNT; i++) {
        a2d->regs[i] = 0;
    }
    /* Scale * 2 */
    a2d->regs[AFFINE2D_REG_A] = 1 << 8;
    a2d->regs[AFFINE2D_REG_D] = 1 << 8;

    a2d->regs[AFFINE2D_REG_F] = 0.0777 * (1 << 14);
    a2d->regs[AFFINE2D_REG_G] = 1 << 14;

    /* Set the center to the middle of the screen */
    a2d->regs[AFFINE2D_REG_CX] = 320 / 2;
    a2d->regs[AFFINE2D_REG_CY] = 240 / 2;
}


void zvb_affine2d_write(zvb_affine2d_t* a2d, uint32_t addr, uint8_t data)
{
    const int16_t value = (int16_t) (a2d->latch | (data << 8));

    if (addr == AFFINE2D_REG_CTRL) {
        a2d->ctrl = data;
    } else if (!a2d->latched) {
        a2d->latch = data;
    } else if (addr <= AFFINE2D_ADDR_REG_CY) {
        a2d->regs[addr - AFFINE2D_REG_A] = value;
        a2d->latched = false;
    } else {
        log_err_printf("[AFFINE2D] Unknown register %x\n", addr);
    }
}

uint8_t zvb_affine2d_read(zvb_affine2d_t* a2d, uint32_t addr)
{
    int* reg = NULL;

    if (addr == AFFINE2D_REG_CTRL) {
        return a2d->ctrl;
    } else if (addr <= AFFINE2D_ADDR_REG_CY) {
        reg = &a2d->regs[addr - AFFINE2D_REG_A];
    } else {
        log_err_printf("[AFFINE2D] Unknown register %x\n", addr);
        return 0;
    }

    if (a2d->latched) {
        a2d->latched = false;
        return (uint8_t) (*reg >> 8);
    }
    return (uint8_t) (*reg & 0xff);
}

const int* zvb_affine2d_matrix(zvb_affine2d_t* a2d)
{
    /* Check if the transformation is enabled */
    if ((a2d->ctrl & AFFINE2D_REG_CTRL_ENABLE)) {
        return a2d->regs + AFFINE2D_REG_A;
    }
    /* Not enabled, returns identity (A B C D) */
    static const int identity[] = { (1 << 8), 0, 0, (1 << 8) };
    return identity;
}

const int* zvb_affine2d_perspective(zvb_affine2d_t* a2d)
{
    /* Check if the transformation is enabled */
    if ((a2d->ctrl & AFFINE2D_REG_CTRL_ENABLE)) {
        return a2d->regs + AFFINE2D_REG_F;
    }
    /* Not enabled, returns identity (F G) Q2.14 */
    static const int identity[] = { 0, (1 << 14) };
    return identity;
}

const int* zvb_affine2d_center(zvb_affine2d_t* a2d)
{
    /* Check if the transformation is enabled */
    if ((a2d->ctrl & AFFINE2D_REG_CTRL_ENABLE)) {
        return a2d->regs + AFFINE2D_REG_CX;
    }
    /* Not enabled, returns identity (CX CY) */
    static const int identity[] = { 0, 0 };
    return identity;
}
