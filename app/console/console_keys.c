/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <string.h>

#include "app/console/console.h"
#include "app/console/console_keys.h"
#include "hw/keyboard.h"
#include "platform/display.h"
#include "utils/helpers.h"
#include "utils/log.h"

typedef struct {
    const char* name;
    int key;
} console_key_t;

/* Named keys. Single letters (A-Z) and digits (0-9) are handled directly. */
static const console_key_t CONSOLE_KEYS[] = {
    /* Cursor / navigation */
    { "UP",          DISPLAY_KEY_UP          },
    { "DOWN",        DISPLAY_KEY_DOWN        },
    { "LEFT",        DISPLAY_KEY_LEFT        },
    { "RIGHT",       DISPLAY_KEY_RIGHT       },
    { "HOME",        DISPLAY_KEY_HOME        },
    { "END",         DISPLAY_KEY_END         },
    { "PAGE_UP",     DISPLAY_KEY_PAGE_UP     },
    { "PAGE_DOWN",   DISPLAY_KEY_PAGE_DOWN   },
    { "INSERT",      DISPLAY_KEY_INSERT      },
    { "DELETE",      DISPLAY_KEY_DELETE      },
    /* Editing / control */
    { "ENTER",       DISPLAY_KEY_ENTER       },
    { "ESC",         DISPLAY_KEY_ESCAPE      },
    { "ESCAPE",      DISPLAY_KEY_ESCAPE      },
    { "SPACE",       DISPLAY_KEY_SPACE       },
    { "BACKSPACE",   DISPLAY_KEY_BACKSPACE   },
    { "TAB",         DISPLAY_KEY_TAB         },
    { "CAPS_LOCK",   DISPLAY_KEY_CAPS_LOCK   },
    /* Modifiers */
    { "SHIFT",       DISPLAY_KEY_LEFT_SHIFT    },
    { "CTRL",        DISPLAY_KEY_LEFT_CONTROL  },
    { "ALT",         DISPLAY_KEY_LEFT_ALT      },
    { "RIGHT_SHIFT", DISPLAY_KEY_RIGHT_SHIFT   },
    { "RIGHT_CTRL",  DISPLAY_KEY_RIGHT_CONTROL },
    { "RIGHT_ALT",   DISPLAY_KEY_RIGHT_ALT     },
    /* Function keys */
    { "F1",  DISPLAY_KEY_F1  },
    { "F2",  DISPLAY_KEY_F2  },
    { "F3",  DISPLAY_KEY_F3  },
    { "F4",  DISPLAY_KEY_F4  },
    { "F5",  DISPLAY_KEY_F5  },
    { "F6",  DISPLAY_KEY_F6  },
    { "F7",  DISPLAY_KEY_F7  },
    { "F8",  DISPLAY_KEY_F8  },
    { "F9",  DISPLAY_KEY_F9  },
    { "F10", DISPLAY_KEY_F10 },
    { "F11", DISPLAY_KEY_F11 },
    { "F12", DISPLAY_KEY_F12 },
    /* Punctuation */
    { "MINUS",         DISPLAY_KEY_MINUS         },
    { "EQUAL",         DISPLAY_KEY_EQUAL         },
    { "COMMA",         DISPLAY_KEY_COMMA         },
    { "PERIOD",        DISPLAY_KEY_PERIOD        },
    { "SLASH",         DISPLAY_KEY_SLASH         },
    { "SEMICOLON",     DISPLAY_KEY_SEMICOLON     },
    { "APOSTROPHE",    DISPLAY_KEY_APOSTROPHE    },
    { "GRAVE",         DISPLAY_KEY_GRAVE         },
    { "LEFT_BRACKET",  DISPLAY_KEY_LEFT_BRACKET  },
    { "RIGHT_BRACKET", DISPLAY_KEY_RIGHT_BRACKET },
    { "BACKSLASH",     DISPLAY_KEY_BACKSLASH     },
    /* Keypad */
    { "KP_0", DISPLAY_KEY_KP_0 },
    { "KP_1", DISPLAY_KEY_KP_1 },
    { "KP_2", DISPLAY_KEY_KP_2 },
    { "KP_3", DISPLAY_KEY_KP_3 },
    { "KP_4", DISPLAY_KEY_KP_4 },
    { "KP_5", DISPLAY_KEY_KP_5 },
    { "KP_6", DISPLAY_KEY_KP_6 },
    { "KP_7", DISPLAY_KEY_KP_7 },
    { "KP_8", DISPLAY_KEY_KP_8 },
    { "KP_9", DISPLAY_KEY_KP_9 },
    { "KP_ADD",      DISPLAY_KEY_KP_ADD      },
    { "KP_SUBTRACT", DISPLAY_KEY_KP_SUBTRACT },
    { "KP_MULTIPLY", DISPLAY_KEY_KP_MULTIPLY },
    { "KP_DIVIDE",   DISPLAY_KEY_KP_DIVIDE   },
    { "KP_DECIMAL",  DISPLAY_KEY_KP_DECIMAL  },
};


static int console_parse_key(const char* name)
{
    if (name == NULL) {
        return -1;
    }

    /* Single character: letters and digits */
    if (name[0] != '\0' && name[1] == '\0') {
        const char c = (char)toupper((unsigned char)name[0]);
        if (c >= 'A' && c <= 'Z') {
            return DISPLAY_KEY_A + (c - 'A');
        }
        if (c >= '0' && c <= '9') {
            return DISPLAY_KEY_ZERO + (c - '0');
        }
    }

    for (unsigned int i = 0; i < DIM(CONSOLE_KEYS); i++) {
        const console_key_t* entry = &CONSOLE_KEYS[i];
        const char* a = name;
        const char* b = entry->name;
        while (*a != '\0' && *b != '\0') {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
                break;
            }
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            return entry->key;
        }
    }
    return -1;
}


void console_key(zeal_t* machine, int argc, char** argv)
{
    const bool release = (strcmp(argv[0], "release") == 0);
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: %s <key>\n", argv[0]);
        return;
    }
    const int key = console_parse_key(argv[1]);
    if (key < 0) {
        log_err_printf("[CONSOLE] Unknown key: '%s'\n", argv[1]);
        return;
    }
    if (release) {
        key_released(&machine->keyboard, (uint16_t)key);
    } else {
        key_pressed(&machine->keyboard, (uint16_t)key);
    }
}


void console_tap(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: tap <key>\n");
        return;
    }
    const int key = console_parse_key(argv[1]);
    if (key < 0) {
        log_err_printf("[CONSOLE] Unknown key: '%s'\n", argv[1]);
        return;
    }
    key_pressed(&machine->keyboard, (uint16_t)key);
    key_released(&machine->keyboard, (uint16_t)key);
}
