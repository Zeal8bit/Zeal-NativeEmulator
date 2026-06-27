#include <stdint.h>
#include <stdio.h>

#include "raylib.h"

#ifdef CONFIG_TARGET_RETROFLAG_GPI2
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "utils/paths.h"
#include "utils/helpers.h"
#include "hw/userport/snes_adapter.h"
#include "hw/userport/snes_adapter/controller.h"

#ifndef ZEAL_ASSETS_DIR
#define ZEAL_ASSETS_DIR "assets"
#endif

#define GAMECONTROLLERDB_DEV_PATH     "assets/resources/gamecontrollerdb.txt"
#define GAMECONTROLLERDB_INSTALL_PATH ZEAL_ASSETS_DIR "/resources/gamecontrollerdb.txt"

#ifdef CONFIG_TARGET_RETROFLAG_GPI2
#define RETROFLAG_GPI2_VENDOR_ID     0x045e
#define RETROFLAG_GPI2_PRODUCT_ID    0x028e
#define RETROFLAG_GPI2_EVENT_COUNT   32

static int s_retroflag_gpi2_fd = -1;
static int s_retroflag_gpi2_dpad_x = 0;
static int s_retroflag_gpi2_dpad_y = 0;

static void retroflag_gpi2_init(void)
{
    char path[32];

    for (int event = 0; event < RETROFLAG_GPI2_EVENT_COUNT; event++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", event);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        struct input_id id;
        if (ioctl(fd, EVIOCGID, &id) == 0 &&
            id.vendor == RETROFLAG_GPI2_VENDOR_ID &&
            id.product == RETROFLAG_GPI2_PRODUCT_ID) {
            struct input_absinfo abs_info;
            if (ioctl(fd, EVIOCGABS(ABS_HAT0X), &abs_info) == 0) {
                s_retroflag_gpi2_dpad_x = abs_info.value;
            }
            if (ioctl(fd, EVIOCGABS(ABS_HAT0Y), &abs_info) == 0) {
                s_retroflag_gpi2_dpad_y = abs_info.value;
            }

            s_retroflag_gpi2_fd = fd;
            printf("[SNES] RetroFlag GPi Case 2 D-Pad found at %s\n", path);
            return;
        }

        close(fd);
    }

    printf("[SNES] RetroFlag GPi Case 2 D-Pad not found\n");
}

static void retroflag_gpi2_update_dpad(void)
{
    if (s_retroflag_gpi2_fd < 0) {
        return;
    }

    struct input_event event;
    while (read(s_retroflag_gpi2_fd, &event, sizeof(event)) == (ssize_t)sizeof(event)) {
        if (event.type != EV_ABS) {
            continue;
        }

        if (event.code == ABS_HAT0X) {
            s_retroflag_gpi2_dpad_x = event.value;
        } else if (event.code == ABS_HAT0Y) {
            s_retroflag_gpi2_dpad_y = event.value;
        }
    }
}
#endif

void snes_controller_init(snes_controller_t* ctrl, uint8_t index)
{
    ctrl->index = index;
    ctrl->port = SNES_PORT_DETACHED;
    ctrl->attached = false;

#ifdef CONFIG_TARGET_RETROFLAG_GPI2
    if (index == 0 && s_retroflag_gpi2_fd < 0) {
        retroflag_gpi2_init();
    }
#endif
}

void snes_controller_deinit(void)
{
#ifdef CONFIG_TARGET_RETROFLAG_GPI2
    if (s_retroflag_gpi2_fd >= 0) {
        close(s_retroflag_gpi2_fd);
        s_retroflag_gpi2_fd = -1;
    }
#endif
}

void snes_controller_load_mappings(void)
{
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

bool snes_controller_available(uint8_t index)
{
    return IsGamepadAvailable(index);
}

const char* snes_controller_name(uint8_t index)
{
    return GetGamepadName(index);
}

int snes_controller_axis_count(uint8_t index)
{
    return GetGamepadAxisCount(index);
}

uint16_t snes_controller_latch(snes_controller_t* ctrl)
{
    int index = ctrl->index;
    uint16_t bits = 0xFFFF;  // no buttons pressed (active low)

    if (IsGamepadAvailable(index)) {
        if (!ctrl->attached) {
            printf("[SNES] \"%s\" is now available\n", snes_controller_name(index));
            ctrl->attached = true;
        }

        // ABXY
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  bits &= ~(1 << SNES_BTN_B);  // B
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))  bits &= ~(1 << SNES_BTN_Y);  // Y
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) bits &= ~(1 << SNES_BTN_A);  // A
        if (IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_UP))    bits &= ~(1 << SNES_BTN_X);  // X

#ifdef CONFIG_TARGET_RETROFLAG_GPI2
        // GPi Case 2 exposes its D-Pad directly through Linux ABS_HAT0X/Y events.
        retroflag_gpi2_update_dpad();
        if (s_retroflag_gpi2_dpad_y < 0) bits &= ~(1 << SNES_BTN_UP);
        if (s_retroflag_gpi2_dpad_y > 0) bits &= ~(1 << SNES_BTN_DOWN);
        if (s_retroflag_gpi2_dpad_x < 0) bits &= ~(1 << SNES_BTN_LEFT);
        if (s_retroflag_gpi2_dpad_x > 0) bits &= ~(1 << SNES_BTN_RIGHT);
#else
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
#endif

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
            printf("[SNES] Controller on port %d no longer available\n", ctrl->port + 1);
            ctrl->attached = false;
        }
    }

    return bits;
}
