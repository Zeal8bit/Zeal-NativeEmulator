#include <time.h>
#include <stdbool.h>
#include "hw/i2c/i2c_device.h"

#define DS1307_ADDR  0x68
#define DS1307_LEN   0x40

typedef struct ds1307_t {
    struct i2c_device_t parent;
    /* Time base in seconds */
    time_t   time_diff;
    /* Current register */
    uint8_t  reg;
    /* Bytes cycles received */
    int      count;
    uint8_t  ram[DS1307_LEN];
    /* Mark whether any time-related register changed */
    bool     changed;
} ds1307_t;


int ds1307_init(ds1307_t* rtc);
