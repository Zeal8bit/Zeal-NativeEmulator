#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "raylib.h"
#include "hw/zeal.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes-adapter/mouse.h"
#include "hw/zvb/zvb.h"
#include "utils/notif.h"

static Vector2 snes_mouse_window_scale(float delta_scale) {
    const int screen_w = GetScreenWidth();
    const int screen_h = GetScreenHeight();
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

    return (Vector2) {
        .x = ((float)ZVB_MAX_RES_WIDTH / draw_w) * delta_scale,
        .y = ((float)ZVB_MAX_RES_HEIGHT / draw_h) * delta_scale,
    };
}

static uint8_t snes_mouse_magnitude(float value) {
    int magnitude = (int)roundf(fabsf(value));

    if (magnitude > SNES_MOUSE_MAG_MASK) {
        magnitude = SNES_MOUSE_MAG_MASK;
    }

    return (uint8_t)magnitude;
}

static bool snes_mouse_cursor_in_rect(Rectangle bounds) {
    Vector2 position = GetMousePosition();

    return position.x >= bounds.x &&
        position.y >= bounds.y &&
        position.x < bounds.x + bounds.width &&
        position.y < bounds.y + bounds.height;
}

static Rectangle snes_mouse_active_bounds(snes_mouse_t* mouse) {
#if CONFIG_ENABLE_DEBUGGER
    if (mouse->machine != NULL && mouse->machine->dbg_enabled && mouse->machine->dbg_ui != NULL) {
        Rectangle bounds;
        if (debugger_ui_main_view_bounds(mouse->machine->dbg_ui, &bounds)) {
            return bounds;
        }
    }
#else
    (void)mouse;
#endif
    return (Rectangle) { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
}

static void snes_mouse_clamp_to_bounds(snes_mouse_t* mouse) {
    Rectangle bounds = snes_mouse_active_bounds(mouse);
    Vector2 position = GetMousePosition();
    int x = (int)roundf(position.x);
    int y = (int)roundf(position.y);
    int min_x = (int)roundf(bounds.x);
    int min_y = (int)roundf(bounds.y);
    int max_x = (int)roundf(bounds.x + bounds.width - 1.0f);
    int max_y = (int)roundf(bounds.y + bounds.height - 1.0f);

    if (x < min_x) x = min_x;
    if (y < min_y) y = min_y;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;

    if (x != (int)roundf(position.x) || y != (int)roundf(position.y)) {
        SetMousePosition(x, y);
    }
}

static void snes_mouse_capture(snes_mouse_t* mouse) {
    mouse->captured = true;
    DisableCursor();
    printf("[SNES] Mouse captured\n");
}

static void snes_mouse_release(snes_mouse_t* mouse) {
    mouse->captured = false;
    EnableCursor();
    printf("[SNES] Mouse released\n");
}

static void snes_mouse_set_bit(uint32_t* bits, uint8_t bit, bool value) {
    // SNES data is active low: logical 1 is driven low, logical 0 is driven high.
    if (value) {
        *bits &= ~(1u << bit);
    } else {
        *bits |= (1u << bit);
    }
}

void snes_mouse_init(snes_mouse_t* mouse) {
    mouse->attached = true;
    mouse->captured = false;
    mouse->machine = NULL;
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
}

void snes_mouse_detach(snes_mouse_t* mouse) {
    mouse->attached = false;
    if (mouse->captured) {
        snes_mouse_release(mouse);
    }
}

void snes_mouse_reset_scale(snes_mouse_t* mouse) {
    mouse->delta_scale = SNES_MOUSE_DELTA_SCALE;
    notif_show("SNES Mouse Scale: x%.2f", mouse->delta_scale);
}

uint32_t snes_mouse_latch(snes_mouse_t* mouse) {
    uint32_t bits = 0xFFFFFFFF;
    Vector2 delta = GetMouseDelta();
    bool cursor_in_bounds = snes_mouse_cursor_in_rect(snes_mouse_active_bounds(mouse));

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        if (mouse->captured) {
            snes_mouse_release(mouse);
        } else if (IsWindowFocused() && cursor_in_bounds) {
            snes_mouse_capture(mouse);
        }
    }

    if (mouse->captured) {
        snes_mouse_clamp_to_bounds(mouse);
    }

    bool active = mouse->captured || cursor_in_bounds;
    if (active) {
        float wheel = GetMouseWheelMove();
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

    if (!active) {
        delta = (Vector2) { 0.0f, 0.0f };
    }

    Vector2 scale = snes_mouse_window_scale(mouse->delta_scale);
    delta.x *= scale.x;
    delta.y *= scale.y;

    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_RIGHT, active && IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    snes_mouse_set_bit(&bits, SNES_MOUSE_SERIAL_LEFT, active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));

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
