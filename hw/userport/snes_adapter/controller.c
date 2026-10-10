#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/paths.h"
#include "utils/helpers.h"
#include "platform/controller.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes_adapter/controller.h"

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

#define GAMECONTROLLERDB_DEV_PATH     "assets/resources/gamecontrollerdb.txt"
#define GAMECONTROLLERDB_INSTALL_PATH ZEAL_ASSETS_DIR "/resources/gamecontrollerdb.txt"

void snes_controller_init(snes_controller_t* ctrl, uint8_t index)
{
    ctrl->index = index;
    ctrl->port = SNES_PORT_DETACHED;
    ctrl->attached = false;
}

void snes_controller_load_mappings(void)
{
#ifdef PLATFORM_WEB
    return;
#endif
    /* Search order: user config dir (~/.zeal8bit/) -> dev path -> installed path */
    char path[PATH_MAX];
    bool found = false;
    const char *config_dir = get_config_dir();
    if (config_dir != NULL) {
        snprintf(path, sizeof(path), "%s/gamecontrollerdb.txt", config_dir);
        found = path_exists(path);
    }
    if (!found) {
        char dir[PATH_MAX];
        get_executable_dir(dir, sizeof(dir));
        snprintf(path, sizeof(path), "%s", dir);
        strncat(path, GAMECONTROLLERDB_DEV_PATH, sizeof(path) - strlen(path) - 1);
        found = path_exists(path);
    }
    if (!found) {
        snprintf(path, sizeof(path), "%s", GAMECONTROLLERDB_INSTALL_PATH);
        found = path_exists(path);
    }
    if (!found) {
        printf("[SNES] gamecontrollerdb.txt not found, using built-in mappings\n");
        return;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("[SNES] Cannot read %s, using built-in mappings\n", path);
        return;
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        printf("[SNES] gamecontrollerdb.txt is empty, skipping\n");
        return;
    }
    char *text = malloc((size_t)size + 1);
    if (text != NULL) {
        const size_t read = fread(text, 1, (size_t)size, file);
        text[read] = '\0';
        if (read > 0) {
            controller_set_mappings(text);
            printf("[SNES] Loaded custom gamepad mappings from %s\n", path);
        }
        free(text);
    }
    fclose(file);
}

bool snes_controller_available(uint8_t index)
{
    return controller_available(index);
}

const char* snes_controller_name(uint8_t index)
{
    return controller_name(index);
}

uint16_t snes_controller_latch(snes_controller_t* ctrl)
{
    int index = ctrl->index;
    uint16_t bits = 0xFFFF;  // no buttons pressed (active low)

    if (controller_available(index)) {
        if (!ctrl->attached) {
            printf("[SNES] \"%s\" is now available\n", snes_controller_name(index));
            ctrl->attached = true;
        }

        // ABXY
        if (controller_button_down(index, CONTROLLER_BUTTON_FACE_DOWN))  bits &= ~(1 << SNES_BTN_B);  // B
        if (controller_button_down(index, CONTROLLER_BUTTON_FACE_LEFT))  bits &= ~(1 << SNES_BTN_Y);  // Y
        if (controller_button_down(index, CONTROLLER_BUTTON_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_A);  // A
        if (controller_button_down(index, CONTROLLER_BUTTON_FACE_UP))    bits &= ~(1 << SNES_BTN_X);  // X

        // D-Pad (buttons)
        if (controller_button_down(index, CONTROLLER_BUTTON_DPAD_UP))    bits &= ~(1 << SNES_BTN_UP);    // Up
        if (controller_button_down(index, CONTROLLER_BUTTON_DPAD_DOWN))  bits &= ~(1 << SNES_BTN_DOWN);  // Down
        if (controller_button_down(index, CONTROLLER_BUTTON_DPAD_LEFT))  bits &= ~(1 << SNES_BTN_LEFT);  // Left
        if (controller_button_down(index, CONTROLLER_BUTTON_DPAD_RIGHT)) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Left thumbstick as D-Pad
        float stick_x = controller_axis(index, CONTROLLER_AXIS_LEFT_X);
        float stick_y = controller_axis(index, CONTROLLER_AXIS_LEFT_Y);
        if (stick_y < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_UP);    // Up
        if (stick_y >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_DOWN);  // Down
        if (stick_x < -SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_LEFT);  // Left
        if (stick_x >  SNES_STICK_DEADZONE) bits &= ~(1 << SNES_BTN_RIGHT); // Right

        // Select/Start
        if (controller_button_down(index, CONTROLLER_BUTTON_SELECT)) bits &= ~(1 << SNES_BTN_SELECT);  // Select
        if (controller_button_down(index, CONTROLLER_BUTTON_START))  bits &= ~(1 << SNES_BTN_START);   // Start

        // L/R
        bool left_trigger = controller_button_down(index, CONTROLLER_BUTTON_SHOULDER_LEFT) ||
            controller_button_down(index, CONTROLLER_BUTTON_TRIGGER_LEFT) ||
            controller_axis(index, CONTROLLER_AXIS_TRIGGER_LEFT) > SNES_TRIGGER_DEADZONE;
        bool right_trigger = controller_button_down(index, CONTROLLER_BUTTON_SHOULDER_RIGHT) ||
            controller_button_down(index, CONTROLLER_BUTTON_TRIGGER_RIGHT) ||
            controller_axis(index, CONTROLLER_AXIS_TRIGGER_RIGHT) > SNES_TRIGGER_DEADZONE;
        if (left_trigger)  bits &= ~(1 << SNES_BTN_L);  // L
        if (right_trigger) bits &= ~(1 << SNES_BTN_R);  // R
    } else {
        if (ctrl->attached) {
            printf("[SNES] Controller on port %d no longer available\n", ctrl->port + 1);
            ctrl->attached = false;
        }
    }

    return bits;
}
