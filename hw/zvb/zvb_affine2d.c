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


static int is_overlfow(float u_i, float v_i)
{
    return u_i < 0 || v_i < 0 || u_i >= 1279.999 || v_i >= 639.999;
}

static int is_overlfow_int(int32_t u_i, int32_t v_i)
{
    return u_i < 0 || v_i < 0 || (u_i >> 8) >= 1280 || (v_i >> 8) >= 640;
}

static void test_values(zvb_affine2d_t* a2d)
{
    a2d->ctrl = AFFINE2D_REG_CTRL_ENABLE;
    a2d->regs[AFFINE2D_REG_A] = 1 << 8;
    a2d->regs[AFFINE2D_REG_D] = 1 << 8;

    a2d->regs[AFFINE2D_REG_A] = 181;
    a2d->regs[AFFINE2D_REG_B] = 181;
    a2d->regs[AFFINE2D_REG_C] = -181;
    a2d->regs[AFFINE2D_REG_D] = 181;

    a2d->regs[AFFINE2D_REG_F] = 0x118;
    a2d->regs[AFFINE2D_REG_G] = 0x01;
    /* Set the center to the middle of the screen */
    a2d->regs[AFFINE2D_REG_CX] = 320 / 2;
    a2d->regs[AFFINE2D_REG_CY] = 240 / 2;

#if TEST_FLOAT
    /* To debug this issue, print all the value for the screen */
    const float cx = a2d->regs[AFFINE2D_REG_CX];
    const float cy = a2d->regs[AFFINE2D_REG_CY];

    const float A = (float) (a2d->regs[AFFINE2D_REG_A]) / 256.0f;
    const float B = (float) (a2d->regs[AFFINE2D_REG_B]) / 256.0f;
    const float C = (float) (a2d->regs[AFFINE2D_REG_C]) / 256.0f;
    const float D = (float) (a2d->regs[AFFINE2D_REG_D]) / 256.0f;

    for (int y = 0; y < 64; y++) {
        float x_0 = -cx;
        float y_0 = y - cy;

        const float z = ((float) a2d->regs[AFFINE2D_REG_F]) / 16384.0f * (float) y
                        + (((float) a2d->regs[AFFINE2D_REG_G]) / 16384.0f);

        float mat_a_div = A / z;
        float mat_b_div = B / z;
        float mat_c_div = C / z;
        float mat_d_div = D / z;

        float k_x = mat_b_div * y_0 + cx;
        float k_y = mat_d_div * y_0 + cy;

        float u_0 = mat_a_div * x_0 + k_x;
        float v_0 = mat_c_div * x_0 + k_y;

        float u_i = u_0;
        float v_i = v_0;

        printf("[%03d] = ", y);
        for (int x = 0; x < 320; x++) {
            if (is_overlfow(u_i, v_i)) {
                printf("X ");
            } else {
                printf("(%f, %f), ", u_i, v_i);
            }
            u_i += mat_a_div;
            v_i += mat_c_div;
        }
        printf("\n");
    }
#else
    /* To debug this issue, print all the value for the screen */
    const int16_t cx = a2d->regs[AFFINE2D_REG_CX];
    const int16_t cy = a2d->regs[AFFINE2D_REG_CY];

    const int16_t A = a2d->regs[AFFINE2D_REG_A];
    const int16_t B = a2d->regs[AFFINE2D_REG_B];
    const int16_t C = a2d->regs[AFFINE2D_REG_C];
    const int16_t D = a2d->regs[AFFINE2D_REG_D];

    for (int y = 0; y < 64; y++) {
        int32_t x_0 = -cx;
        int32_t y_0 = y - cy;

        const int32_t z = a2d->regs[AFFINE2D_REG_F] * y + a2d->regs[AFFINE2D_REG_G];

        int16_t mat_a_div = (((int64_t) A) << 14) / z;
        int16_t mat_b_div = (((int64_t) B) << 14) / z;
        int16_t mat_c_div = (((int64_t) C) << 14) / z;
        int16_t mat_d_div = (((int64_t) D) << 14) / z;

        // printf("A: %x / %x = %x\n", A, z, mat_a_div);
        // printf("C: %x / %x = %x\n", C, z, mat_c_div);
        // printf("C: %f / %f = %f\n", C / 256.0, z / (float) (1<<14), (C / 256.0) / (z / (float) (1<<14)));

        int32_t k_x = mat_b_div * y_0 + (cx << 8);
        int32_t k_y = mat_d_div * y_0 + (cy << 8);

        int32_t u_0 = mat_a_div * x_0 + k_x;
        int32_t v_0 = mat_c_div * x_0 + k_y;

        int32_t u_i = u_0;
        int32_t v_i = v_0;

        printf("Y = %d\n", y);
        printf("x_0 = %x\n", x_0);
        printf("y_0 = %x\n", y_0);
        printf("k_x = %x\n", k_x);
        printf("k_y = %x\n", k_y);

        printf("z = %x\n", z);
        printf("mat_a_div: %x\n", mat_a_div);
        printf("mat_b_div: %x\n", mat_b_div);
        printf("mat_c_div: %x\n", mat_c_div);
        printf("mat_d_div: %x\n", mat_d_div);

        printf("[%03d] = ", y);
        for (int x = 0; x < 320; x++) {
            // if (is_overlfow_int(u_i, v_i)) {
            //     printf("O ");
            // } else
            {
                printf("[%d]=(%08x, %08x) ", x, u_i, v_i);
            }
            u_i += mat_a_div;
            v_i += mat_c_div;
        }
        printf("\n");
    }
#endif
}

void zvb_affine2d_init(zvb_affine2d_t* a2d)
{
    a2d->ctrl = 0;
    for (int i = 0; i < AFFINE2D_REG_COUNT; i++) {
        a2d->regs[i] = 0;
    }
    a2d->regs[AFFINE2D_REG_A] = 1 << 8;
    a2d->regs[AFFINE2D_REG_D] = 1 << 8;

    a2d->regs[AFFINE2D_REG_G] = 1 << 14;
}

static void print_matrix(zvb_affine2d_t* a2d)
{
    printf("A = %04x\n", a2d->regs[0]);
    printf("B = %04x\n", a2d->regs[1]);
    printf("C = %04x\n", a2d->regs[2]);
    printf("D = %04x\n", a2d->regs[3]);

    printf("F = %04x\n", a2d->regs[4]);
    printf("G = %04x\n", a2d->regs[5]);

    printf("CX = %04x\n", a2d->regs[6]);
    printf("CY = %04x\n", a2d->regs[7]);
}

void zvb_affine2d_write(zvb_affine2d_t* a2d, uint32_t addr, uint8_t data)
{
    const int16_t value = (int16_t) (a2d->latch | (data << 8));

    if (addr == AFFINE2D_REG_CTRL) {
        a2d->ctrl = data;
    } else if (!a2d->latched) {
        a2d->latched = true;
        a2d->latch = data;
    } else if (addr <= AFFINE2D_ADDR_REG_CY) {
        print_matrix(a2d);
        a2d->regs[addr - AFFINE2D_ADDR_REG_A] = value;
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
        reg = &a2d->regs[addr - AFFINE2D_ADDR_REG_A];
    } else {
        log_err_printf("[AFFINE2D] Unknown register %x\n", addr);
        return 0;
    }

    if (a2d->latched) {
        a2d->latched = false;
        return (uint8_t) (*reg >> 8);
    } else {
        a2d->latched = true;
        return (uint8_t) (*reg & 0xff);
    }
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
