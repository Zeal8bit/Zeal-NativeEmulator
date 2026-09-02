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
#include "utils/helpers.h"
#include "utils/log.h"

typedef struct {
    const char* name;
    int key;
} console_key_t;

/* Named keys. Single letters (A-Z) and digits (0-9) are handled directly. */
static const console_key_t CONSOLE_KEYS[] = {
    /* Cursor / navigation */
    { "UP",          KEY_UP          },
    { "DOWN",        KEY_DOWN        },
    { "LEFT",        KEY_LEFT        },
    { "RIGHT",       KEY_RIGHT       },
    { "HOME",        KEY_HOME        },
    { "END",         KEY_END         },
    { "PAGE_UP",     KEY_PAGE_UP     },
    { "PAGE_DOWN",   KEY_PAGE_DOWN   },
    { "INSERT",      KEY_INSERT      },
    { "DELETE",      KEY_DELETE      },
    /* Editing / control */
    { "ENTER",       KEY_ENTER       },
    { "ESC",         KEY_ESCAPE      },
    { "ESCAPE",      KEY_ESCAPE      },
    { "SPACE",       KEY_SPACE       },
    { "BACKSPACE",   KEY_BACKSPACE   },
    { "TAB",         KEY_TAB         },
    { "CAPS_LOCK",   KEY_CAPS_LOCK   },
    /* Modifiers */
    { "SHIFT",       KEY_LEFT_SHIFT    },
    { "CTRL",        KEY_LEFT_CONTROL  },
    { "ALT",         KEY_LEFT_ALT      },
    { "RIGHT_SHIFT", KEY_RIGHT_SHIFT   },
    { "RIGHT_CTRL",  KEY_RIGHT_CONTROL },
    { "RIGHT_ALT",   KEY_RIGHT_ALT     },
    /* Function keys */
    { "F1",  KEY_F1  },
    { "F2",  KEY_F2  },
    { "F3",  KEY_F3  },
    { "F4",  KEY_F4  },
    { "F5",  KEY_F5  },
    { "F6",  KEY_F6  },
    { "F7",  KEY_F7  },
    { "F8",  KEY_F8  },
    { "F9",  KEY_F9  },
    { "F10", KEY_F10 },
    { "F11", KEY_F11 },
    { "F12", KEY_F12 },
    /* Punctuation */
    { "MINUS",         KEY_MINUS         },
    { "EQUAL",         KEY_EQUAL         },
    { "COMMA",         KEY_COMMA         },
    { "PERIOD",        KEY_PERIOD        },
    { "SLASH",         KEY_SLASH         },
    { "SEMICOLON",     KEY_SEMICOLON     },
    { "APOSTROPHE",    KEY_APOSTROPHE    },
    { "GRAVE",         KEY_GRAVE         },
    { "LEFT_BRACKET",  KEY_LEFT_BRACKET  },
    { "RIGHT_BRACKET", KEY_RIGHT_BRACKET },
    { "BACKSLASH",     KEY_BACKSLASH     },
    /* Keypad */
    { "KP_0", KEY_KP_0 },
    { "KP_1", KEY_KP_1 },
    { "KP_2", KEY_KP_2 },
    { "KP_3", KEY_KP_3 },
    { "KP_4", KEY_KP_4 },
    { "KP_5", KEY_KP_5 },
    { "KP_6", KEY_KP_6 },
    { "KP_7", KEY_KP_7 },
    { "KP_8", KEY_KP_8 },
    { "KP_9", KEY_KP_9 },
    { "KP_ADD",      KEY_KP_ADD      },
    { "KP_SUBTRACT", KEY_KP_SUBTRACT },
    { "KP_MULTIPLY", KEY_KP_MULTIPLY },
    { "KP_DIVIDE",   KEY_KP_DIVIDE   },
    { "KP_DECIMAL",  KEY_KP_DECIMAL  },
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
            return KEY_A + (c - 'A');
        }
        if (c >= '0' && c <= '9') {
            return KEY_ZERO + (c - '0');
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
