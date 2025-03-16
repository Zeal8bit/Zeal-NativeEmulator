#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include "hw/i2c/i2c_device.h"

#define AT24C512_ADDR    0x50
#define AT24C512_SIZE    0x10000  // 64KB
#define AT24C512_PAGE    128      // Page size: 128 bytes


typedef struct at24c512_t {
    struct i2c_device_t parent;
    uint8_t     data[AT24C512_SIZE];
    uint16_t    address;
    int         sector_written;
    int         count;
    bool        writing;
    FILE*       file;
} at24c512_t;


/**
 * @brief Initialize the emulated EEPROM, if a file is passed, it will
 * be loaded inside the eeprom.
 */
int at24c512_init(at24c512_t* eeprom, const char* filename);


/**
 * @brief Deinitialize the emulated EEPROM. This needs to be called to be sure
 * that the changes will be flushed to the opened file.
 */
void at24c512_deinit(at24c512_t* eeprom);
