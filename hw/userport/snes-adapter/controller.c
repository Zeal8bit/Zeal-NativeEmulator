#include <stdint.h>
#include <stdio.h>

#include "raylib.h"
#include "utils/paths.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes-adapter/controller.h"

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

#define GAMECONTROLLERDB_DEV_PATH     "assets/resources/gamecontrollerdb.txt"
#define GAMECONTROLLERDB_INSTALL_PATH ZEAL_ASSETS_DIR "/resources/gamecontrollerdb.txt"

void snes_controller_init(snes_controller_t* ctrl, uint8_t index) {
    ctrl->bits = 0x0000;
    ctrl->index = index;
    ctrl->attached = false;
}

void snes_controller_load_mappings(void) {
    // Search order: user config dir (~/.zeal8bit/) -> dev path -> installed path
    const char *config_dir = get_config_dir();
    const char *db_path = config_dir
        ? TextFormat("%s/gamecontrollerdb.txt", config_dir)
        : NULL;

    if (!db_path || !FileExists(db_path)) {
        db_path = TextFormat("%s" GAMECONTROLLERDB_DEV_PATH, GetApplicationDirectory());
    }
    if (!FileExists(db_path)) {
        db_path = GAMECONTROLLERDB_INSTALL_PATH;
    }

    if (FileExists(db_path)) {
        char* custom_mapping = LoadFileText(db_path);
        if (custom_mapping != NULL && custom_mapping[0] != '\0') {
            SetGamepadMappings(custom_mapping);
            printf("[SNES] Loaded custom gamepad mappings from %s\n", db_path);
        } else {
            printf("[SNES] gamecontrollerdb.txt is empty, skipping\n");
        }
        UnloadFileText(custom_mapping);
    } else {
        printf("[SNES] gamecontrollerdb.txt not found, using built-in mappings\n");
    }
}

bool snes_controller_available(uint8_t index) {
    return IsGamepadAvailable(index);
}

const char* snes_controller_name(uint8_t index) {
    return GetGamepadName(index);
}

uint16_t snes_controller_latch(snes_controller_t* ctrl) {
    int index = ctrl->index;
    uint16_t bits = 0xFFFF;  // no buttons pressed (active low)

    if (IsGamepadAvailable(index)) {
        if (!ctrl->attached) {
            printf("[SNES] Gamepad %d is now available\n", index);
            ctrl->attached = true;
        }

        // ABXY
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  bits &= ~(1 << SNES_BTN_B);  // B
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))  bits &= ~(1 << SNES_BTN_Y);  // Y
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_A);  // A
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_UP))    bits &= ~(1 << SNES_BTN_X);  // X

        // D-Pad (buttons)
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_UP))    bits &= ~(1 << SNES_BTN_UP);    // Up
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  bits &= ~(1 << SNES_BTN_DOWN);  // Down
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  bits &= ~(1 << SNES_BTN_LEFT);  // Left
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Left thumbstick as D-Pad
        float stick_x = GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_X);
        float stick_y = GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_Y);
        if (stick_y < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_UP);    // Up
        if (stick_y >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_DOWN);  // Down
        if (stick_x < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_LEFT);  // Left
        if (stick_x >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Select/Start
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_LEFT))  bits &= ~(1 << SNES_BTN_SELECT);  // Select
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_RIGHT)) bits &= ~(1 << SNES_BTN_START);   // Start

        // L/R
        bool left_trigger = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
            IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_2) ||
            GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_TRIGGER) > SNES_TRIGGER_DEADZONE;
        bool right_trigger = IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ||
            IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_2) ||
            GetGamepadAxisMovement(index, GAMEPAD_AXIS_RIGHT_TRIGGER) > SNES_TRIGGER_DEADZONE;
        if (left_trigger)  bits &= ~(1 << SNES_BTN_L);  // L
        if (right_trigger) bits &= ~(1 << SNES_BTN_R);  // R
    } else {
        if (ctrl->attached) {
            ctrl->attached = false;
            printf("[SNES] Gamepad %d no longer available\n", index);
        }
    }

    return bits;
}
