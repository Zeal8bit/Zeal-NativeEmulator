#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "platform/display.h"
#include "hw/zeal.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes_adapter/mouse.h"
#include "hw/zvb/zvb.h"
#include "utils/notif.h"

static display_vec2_t snes_mouse_window_scale(float delta_scale)
{
    const int screen_w = display_width();
    const int screen_h = display_height();
    const float texture_ratio = (float)ZVB_MAX_RES_WIDTH / ZVB_MAX_RES_HEIGHT;
    const float screen_ratio = (float)screen_w / screen_h;
    int draw_w = ZVB_MAX_RES_WIDTH;
    int draw_h = ZVB_MAX_RES_HEIGHT;

    if (texture_ratio > screen_ratio) {
        draw_w = screen_w;
        draw_h = (int)(screen_w / texture_ratio);
    } else {
        draw_h = screen_h;
        draw_w = (int)(screen_h * texture_ratio);
    }

    return (display_vec2_t) {
        .x = ((float)ZVB_MAX_RES_WIDTH / draw_w) * delta_scale,
        .y = ((float)ZVB_MAX_RES_HEIGHT / draw_h) * delta_scale,
    };
}

static uint8_t snes_mouse_magnitude(float value)
{
    int magnitude = (int)roundf(fabsf(value));

    if (magnitude > SNES_MOUSE_MAG_MASK) {
        magnitude = SNES_MOUSE_MAG_MASK;
    }

    return (uint8_t)magnitude;
}

static bool snes_mouse_cursor_in_rect(display_rect_t bounds)
{
    display_vec2_t position = display_mouse_position();

    return position.x >= bounds.x &&
        position.y >= bounds.y &&
        position.x < bounds.x + bounds.width &&
        position.y < bounds.y + bounds.height;
}

static display_rect_t snes_mouse_active_bounds(snes_mouse_t* mouse)
{
    (void)mouse;
    return (display_rect_t) { 0, 0, (float)display_width(), (float)display_height() };
}

static void snes_mouse_capture(snes_mouse_t* mouse)
{
    mouse->captured = true;
    display_mouse_capture(true);
    printf("[SNES] Mouse captured\n");
}

static void snes_mouse_release(snes_mouse_t* mouse)
{
    mouse->captured = false;
    display_mouse_capture(false);
    printf("[SNES] Mouse released\n");
}

static inline void snes_mouse_set_bit(uint32_t* bits, uint8_t bit, bool value)
{
    // SNES data is active low: logical 1 is driven low, logical 0 is driven high.
    if (value) {
        *bits &= ~(1u << bit);
    } else {
        *bits |= (1u << bit);
    }
}

void snes_mouse_init(snes_mouse_t* mouse)
{
    mouse->attached = true;
    mouse->captured = false;
    mouse->capture_button_down = false;
    mouse->machine = NULL;
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
}

void snes_mouse_detach(snes_mouse_t* mouse)
{
    mouse->attached = false;
    mouse->capture_button_down = false;
    if (mouse->captured) {
        snes_mouse_release(mouse);
    }
}

void snes_mouse_reset_scale(snes_mouse_t* mouse)
{
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
    notif_show("SNES Mouse Scale: x%.2f", mouse->delta_scale);
}

void snes_mouse_update(snes_mouse_t* mouse)
{
#if CONFIG_ENABLE_DEBUGGER
    if(mouse->machine && mouse->machine->dbg_frontend_visible) return;
#endif
    bool capture_button_down = display_mouse_down(DISPLAY_MOUSE_MIDDLE);

    if (!mouse->attached) {
        if (mouse->captured) {
            snes_mouse_release(mouse);
        }
        mouse->capture_button_down = capture_button_down;
        return;
    }

    bool cursor_in_bounds = snes_mouse_cursor_in_rect(snes_mouse_active_bounds(mouse));
    if (capture_button_down && !mouse->capture_button_down) {
        if (mouse->captured) {
            snes_mouse_release(mouse);
        } else if (display_focused() && cursor_in_bounds) {
            snes_mouse_capture(mouse);
        }
    }
    mouse->capture_button_down = capture_button_down;

    bool active = mouse->captured || cursor_in_bounds;
    if (active) {
        float wheel = display_mouse_wheel();
        if (wheel != 0.0f) {
            mouse->delta_scale += wheel * SNES_MOUSE_DELTA_SCALE_STEP;
            if (mouse->delta_scale < SNES_MOUSE_DELTA_SCALE_MIN) {
                mouse->delta_scale = SNES_MOUSE_DELTA_SCALE_MIN;
            } else if (mouse->delta_scale > SNES_MOUSE_DELTA_SCALE_MAX) {
                mouse->delta_scale = SNES_MOUSE_DELTA_SCALE_MAX;
            }
            notif_show("SNES Mouse Speed: x%.2f", mouse->delta_scale);
        }
    }
}

uint32_t snes_mouse_latch(snes_mouse_t* mouse)
{
    uint32_t bits = 0xFFFFFFFF;
    display_vec2_t delta;
    bool active,left,right;
#if CONFIG_ENABLE_DEBUGGER
    zeal_t* machine=mouse->machine;
    if(machine && machine->dbg_frontend_visible) {
        delta=(display_vec2_t){machine->frontend_mouse_dx*mouse->delta_scale,machine->frontend_mouse_dy*mouse->delta_scale};
        machine->frontend_mouse_dx=machine->frontend_mouse_dy=0;
        active=debugger_ui_main_view_focused(machine->dbg_ui);
        left=(machine->frontend_mouse_buttons&1)!=0;right=(machine->frontend_mouse_buttons&2)!=0;
    } else
#endif
    {
        display_mouse_delta(&delta.x, &delta.y);
        active=mouse->captured||snes_mouse_cursor_in_rect(snes_mouse_active_bounds(mouse));
        display_vec2_t scale=snes_mouse_window_scale(mouse->delta_scale);delta.x*=scale.x;delta.y*=scale.y;
        left=display_mouse_down(DISPLAY_MOUSE_LEFT);right=display_mouse_down(DISPLAY_MOUSE_RIGHT);
    }
    if(!active)delta=(display_vec2_t){0,0};
    snes_mouse_set_bit(&bits,SNES_MOUSE_SERIAL_RIGHT,active&&right);
    snes_mouse_set_bit(&bits,SNES_MOUSE_SERIAL_LEFT,active&&left);

    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SPEED_LSB, SNES_MOUSE_DEFAULT_SPEED & 0x01);
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SPEED_MSB, (SNES_MOUSE_DEFAULT_SPEED >> 1) & 0x01);
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_SIGNATURE, true);

    uint8_t y_mag = snes_mouse_magnitude(delta.y);
    uint8_t x_mag = snes_mouse_magnitude(delta.x);

    snes_mouse_set_bit(&bits, SNES_MOUSE_Y_SIGN_BIT, delta.y < 0.0f);
    for (uint8_t bit = 0; bit < 7; bit++) {
        snes_mouse_set_bit(&bits, SNES_MOUSE_Y_MAG_SHIFT + bit, (y_mag >> (6 - bit)) & 0x01);
    }

    snes_mouse_set_bit(&bits, SNES_MOUSE_X_SIGN_BIT, delta.x < 0.0f);
    for (uint8_t bit = 0; bit < 7; bit++) {
        snes_mouse_set_bit(&bits, SNES_MOUSE_X_MAG_SHIFT + bit, (x_mag >> (6 - bit)) & 0x01);
    }

    return bits;
}
