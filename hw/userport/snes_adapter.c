#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "raylib.h"
#include "utils/helpers.h"
#include "hw/userport/snes_adapter.h"
#include "hw/pio.h"
#include "hw/zeal.h"
#include "utils/paths.h"

static const char* customMappingFilename = "resources/gamecontrollerdb.txt";

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio)
{
    int index = 0;
    snes_adapter->size = 0x00;
    snes_adapter->pio = pio;

    snes_adapter->bits = 0x0000;
    snes_adapter->bitCounter = 0;
    snes_adapter->index = index;

    char path[PATH_MAX];
    if(get_install_dir_file(path, customMappingFilename) == 0) {
        printf("[SNES] Could not open %s\n", customMappingFilename);
        return -1;
    }

    if (FileExists(path)) {
        char* custom_mapping = LoadFileText(path);
        SetGamepadMappings(custom_mapping);
        UnloadFileText(custom_mapping);
        printf("[SNES] Loaded custom gamepad mappings from %s\n", customMappingFilename);
    }

    if(IsGamepadAvailable(index)) {
        printf("[SNES] Found \"%s\"\n", GetGamepadName(index));
        snes_adapter_attach(snes_adapter, index);
    } else {
        printf("[SNES] No gamepads found\n");
    }

    return 0;
}

void snes_adapter_attach(snes_adapter_t *snes_adapter, uint8_t index) {
    snes_adapter->index = index;
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_LATCH, 1, snes_adapter_latch);
    pio_listen_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK, 1, snes_adapter_clock);

    printf("[SNES] Attached \"%s\" to port %d\n", GetGamepadName(index), index);
}

void snes_adapter_detatch(snes_adapter_t *snes_adapter) {
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_LATCH);
    pio_unlisten_a_pin_change(snes_adapter->pio, SNES_IO_CLOCK);
}

void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;

    zeal_t *machine = pio->machine;
    snes_adapter_t *snes_adapter = &machine->snes_adapter;
    int index = snes_adapter->index;
    printf("[SNES] latch: \"%s\" %d\n", GetGamepadName(index), index);

    uint16_t bits = 0xFFFF; // no buttons pressed
    snes_adapter->bitCounter = 0;

    int lastButton = GetGamepadButtonPressed();
    printf("[SNES] lastButton %d\n", lastButton);

    // nnnnRLXArlduSsYB
    // 1111110111011101

    if(IsGamepadAvailable(index)) {
        // ABXY
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            printf("[SNES] B pressed\n");
            bits &= ~(1 << 0);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) {
            printf("[SNES] Y pressed\n");
            bits &= ~(1 << 1);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            printf("[SNES] A pressed\n"); // confirmed
            bits &= ~(1 << 8);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_UP)) {
            printf("[SNES] X pressed\n"); // confirmed
            bits &= ~(1 << 9);
        }

        // D-Pad
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            printf("[SNES] Up pressed\n"); // confirmed
            bits &= ~(1 << 4);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
            printf("[SNES] Down pressed\n"); // confirmed
            bits &= ~(1 << 5);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
            printf("[SNES] Left pressed\n"); // confirmed
            bits &= ~(1 << 6);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
            printf("[SNES] Right pressed\n"); // confirmed
            bits &= ~(1 << 7);
        }

        // Select/Start
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_LEFT)) {
            printf("[SNES] Select pressed\n");
            bits &= ~(1 << 2);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
            printf("[SNES] Start pressed\n");
            bits &= ~(1 << 3);
        }

        // L/R
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
            printf("[SNES] L pressed\n");
            bits &= ~(1 << 10);
        }
        if(IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
            printf("[SNES] R pressed\n");
            bits &= ~(1 << 11);
        }
    } else {
        printf("[SNES] Gamepad %d no longer available\n", index);
        bits = 0xFFFF;
    }

    printf("[SNES] bits %04x\n", bits);
    snes_adapter->bits = bits;
}

void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit) {
    // (void)pio;
    (void)pin;
    (void)bit;
    zeal_t *machine = pio->machine;
    snes_adapter_t *snes_adapter = &machine->snes_adapter;

    snes_adapter->bitCounter++;
    uint16_t value = (snes_adapter->bits >> snes_adapter->bitCounter) & 0x01;
    pio_set_a_pin(pio, SNES_IO_DATA_1, value);

    printf("[SNES] clock: bits: %04x value: %d\n", snes_adapter->bits, value);
}
