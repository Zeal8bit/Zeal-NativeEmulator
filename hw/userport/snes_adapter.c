#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "raylib.h"
#include "utils/helpers.h"
#include "utils/paths.h"
#include "hw/userport/snes_adapter.h"
#include "hw/pio.h"
#include "hw/zeal.h"

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

#define GAMECONTROLLERDB_DEV_PATH     "assets/resources/gamecontrollerdb.txt"
#define GAMECONTROLLERDB_INSTALL_PATH ZEAL_ASSETS_DIR "/resources/gamecontrollerdb.txt"

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio) {
    snes_adapter->size = 0x00;
    snes_adapter->pio = pio;

    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        snes_adapter->controllers[i].bits = 0x0000;
        snes_adapter->controllers[i].index = i;
        snes_adapter->controllers[i].attached = false;
    }

    // Search order: user config dir (~/.zeal8bit/) → dev path → installed path
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

    for (uint8_t i = 0; i < SNES_CONTROLLER_COUNT; i++) {
        if (IsGamepadAvailable(i)) {
            printf("[SNES] Found \"%s\"\n", GetGamepadName(i));
            snes_adapter_attach(snes_adapter, i);
        }
    }

    return 0;
}

void snes_adapter_attach(snes_adapter_t* snes_adapter, uint8_t index) {
    snes_adapter->controllers[index].index = index;
    snes_adapter->controllers[index].attached = true;
    // Listeners are shared for both controllers; registering again is safe (overwrites same slot)
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK, 1, snes_adapter_clock);

    printf("[SNES] Attached \"%s\" to port %d\n", GetGamepadName(index), index);
}

void snes_adapter_detatch(snes_adapter_t* snes_adapter) {
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}

static uint16_t snes_read_gamepad(snes_adapter_t* snes_adapter, uint8_t port) {
    snes_controller_t* ctrl = &snes_adapter->controllers[port];
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
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_1))  bits &= ~(1 << SNES_BTN_L);  // L
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) bits &= ~(1 << SNES_BTN_R);  // R
    } else {
        if (ctrl->attached) {
            ctrl->attached = false;
            printf("[SNES] Gamepad %d no longer available\n", index);
        }
    }

    return bits;
}

void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    // Snapshot button state for both controllers simultaneously on latch
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        snes_adapter->controllers[port].bits = snes_read_gamepad(snes_adapter, port);
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->controllers[0].bits & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->controllers[1].bits & 0x01);
}

void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;

    zeal_t* machine = pio->machine;
    snes_adapter_t* snes_adapter = &machine->snes_adapter;

    // Advance both controllers' bit streams in lock-step
    for (uint8_t port = 0; port < SNES_CONTROLLER_COUNT; port++) {
        snes_adapter->controllers[port].bits >>= 1;
    }

    pio_set_a_pin(pio, SNES_IO_DATA_1, snes_adapter->controllers[0].bits & 0x01);
    pio_set_a_pin(pio, SNES_IO_DATA_2, snes_adapter->controllers[1].bits & 0x01);

    // printf("[SNES] clock: bits[0]: %04x bits[1]: %04x\n", snes_adapter->bits[0], snes_adapter->bits[1]);
}
