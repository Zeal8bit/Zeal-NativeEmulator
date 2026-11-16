#include "frontend/wasm/snes_adapter.h"
#include <stdint.h>
#include <stdio.h>

#include "raylib.h"
#include "utils/helpers.h"
#include "hw/userport/snes_adapter.h"
#include "frontend/wasm/controller.h"

void snes_controller_init(snes_controller_t* ctrl, uint8_t index)
{
    ctrl->index = index;
    ctrl->port = SNES_PORT_DETACHED;
    ctrl->attached = false;
}

bool snes_controller_available(uint8_t index)
{
    return IsGamepadAvailable(index);
}

const char* snes_controller_name(uint8_t index)
{
    return GetGamepadName(index);
}

uint16_t snes_controller_latch(snes_controller_t* ctrl)
{
    int index = ctrl->index;
    uint16_t bits = 0xFFFF; // no buttons pressed (active low)

    if (IsGamepadAvailable(index)) {
        if (!ctrl->attached) {
            printf("[SNES] \"%s\" is now available\n", snes_controller_name(index));
            ctrl->attached = true;
        }

        // ABXY
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) bits &= ~(1 << SNES_BTN_B);  // B
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) bits &= ~(1 << SNES_BTN_Y);  // Y
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_A); // A
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_UP)) bits &= ~(1 << SNES_BTN_X);    // X

        // D-Pad (buttons)
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_UP)) bits &= ~(1 << SNES_BTN_UP);       // Up
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) bits &= ~(1 << SNES_BTN_DOWN);   // Down
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) bits &= ~(1 << SNES_BTN_LEFT);   // Left
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Left thumbstick as D-Pad
        float stick_x = GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_X);
        float stick_y = GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_Y);
        if (stick_y < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_UP);   // Up
        if (stick_y > SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_DOWN);  // Down
        if (stick_x < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_LEFT); // Left
        if (stick_x > SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Select/Start
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_LEFT)) bits &= ~(1 << SNES_BTN_SELECT); // Select
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_RIGHT)) bits &= ~(1 << SNES_BTN_START); // Start

        // L/R
        bool left_trigger = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
                            IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_2) ||
                            GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_TRIGGER) > SNES_TRIGGER_DEADZONE;
        bool right_trigger = IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ||
                             IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_2) ||
                             GetGamepadAxisMovement(index, GAMEPAD_AXIS_RIGHT_TRIGGER) > SNES_TRIGGER_DEADZONE;
        if (left_trigger) bits &= ~(1 << SNES_BTN_L);  // L
        if (right_trigger) bits &= ~(1 << SNES_BTN_R); // R
    } else {
        if (ctrl->attached) {
            printf("[SNES] Controller on port %d no longer available\n", ctrl->port + 1);
            ctrl->attached = false;
        }
    }

    return bits;
}
