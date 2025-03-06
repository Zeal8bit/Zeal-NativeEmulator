#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
#include "hw/z80.h"
#include "hw/device.h"
#include "hw/mmu.h"
#include "hw/flash.h"
#include "hw/ram.h"
#include "hw/zvb/zvb.h"
#include "hw/pio.h"
#include "hw/keyboard.h"
#include "hw/uart.h"
#include "hw/hostfs.h"
#include "debugger/debugger_ui.h"

typedef uint8_t dev_idx_t;

/* Size of the memory space */
#define MEM_SPACE_SIZE (4 * 1024 * 1024)
/* Granularity of the memory space (smallest page a device can be allocated on) */
#define MEM_SPACE_ALIGN (16 * 1024)
/* Size of the memory mapping that will contain our devices */
#define MEM_MAPPING_SIZE ((MEM_SPACE_SIZE) / (MEM_SPACE_ALIGN))

/**
 * @brief Size of the I/O space, considering that it has a granularity of a single byte
 */
#define IO_MAPPING_SIZE 256


/**
 * @brief Maximum number of devices that can be attached to the computer. This is purely
 * arbitrary and not a hardware limitation. The goal is to reduce the footprint in memory.
 */
#define ZEAL_MAX_DEVICE_COUNT 32


/**
 * @brief Macros related to RayLib window
 */
#define WIN_VISIBLE_WIDTH   (ZVB_MAX_RES_WIDTH  * 2)
#define WIN_VISIBLE_HEIGHT  (ZVB_MAX_RES_HEIGHT * 2)
#define WIN_NAME            "Zeal 8-bit Computer"
#define WIN_LOG_LEVEL       LOG_WARNING


/**
 * @brief Each device needs to be associated to the physical page where its mapping starts.
 * Since we have at most 256 pages, we can use a single byte for that
 */
typedef struct {
    device_t* dev;
    int page_from;
} map_entry_t;

struct zeal_t {
    /* Memory regions related, the I/O space's granularity is a single byte */
    map_entry_t io_mapping[IO_MAPPING_SIZE];
    map_entry_t mem_mapping[MEM_MAPPING_SIZE];

    z80     cpu;
    mmu_t   mmu;
    flash_t rom;
    ram_t   ram;
    zvb_t   zvb;
    pio_t   pio;
    keyboard_t keyboard;
    uart_t uart;

    zeal_hostfs_t hostfs;

    /* Debugger related */
    bool             dbg_enabled;
    RenderTexture2D  zvb_out;
    dbg_state_t      dbg_state;
    dbg_t            dbg;
    struct dbg_ui_t* dbg_ui;
};

typedef struct zeal_t zeal_t;


typedef struct {
    bool dbg;
    const char* map_file;
} zeal_opt_t;


/**
 * @brief Initialize the Zeal 8-bit Computer virtual machine with optional parameters
 */
int zeal_init(zeal_t* machine, zeal_opt_t* options);


/**
 * @brief Run the virtual machine, won't return until the emulation is terminated
 */
int zeal_run(zeal_t* machine);

/**
 * @brief Enable Zeal Debugger View
 */
int zeal_debug_enable(zeal_t* machine);
/**
 * @brief Disable Zeal Debugger View
 */
int zeal_debug_disable(zeal_t* machine);
