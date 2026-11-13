/* Check visibility at each device completion, leaving both FILE streams open.
 * Observations use browser OPFS, never libc reads/seeks/flushes on these files. */
#include <assert.h>
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include "hw/i2c/at24c512.h"
#include "utils/config.h"
#include "../../hw/zvb/zvb_spi.c"

#define OBSERVATION_SLEEP_MS 50

config_t config;
static at24c512_t eeprom;
static zvb_spi_t spi;

EM_ASYNC_JS(void, observe_write, (int device, int accepted), {
    await Module.observeWrite(device, accepted);
});

int main(void)
{
    assert(wasmfs_create_directory("/opfs", 0777, wasmfs_create_opfs_backend()) == 0);
    assert(at24c512_init(&eeprom, "/opfs/zeal-completed-write-test/eeprom.img") == 0);
    zvb_spi_init(&spi);
    assert(zvb_spi_load_tf_image(&spi, "/opfs/zeal-completed-write-test/tf.img") == 0);
    spi.tf_cs = 1;
    spi.tf.state = TF_WRITE_BLOCK_WAIT_TOK;
    assert(fseek(spi.tf.img, TF_BLK_SIZE, SEEK_SET) == 0);
    zvb_tf_receive_byte(&spi, 0xfe);
    for (int i = 0; i < TF_BLK_SIZE; i++)
    {
        zvb_tf_receive_byte(&spi, (uint8_t)(i * 13 + 17));
    }
    zvb_tf_receive_byte(&spi, 0); // CRC bytes finish the submitted sector.
    zvb_tf_receive_byte(&spi, 0);
    observe_write(0, spi.tf.reply[0] == TF_WRITE_ACCEPTED);

    eeprom.parent.start(&eeprom.parent);
    eeprom.parent.write(&eeprom.parent, 1); // Page starts at 0x100.
    eeprom.parent.write(&eeprom.parent, 0);
    for (int i = 0; i < AT24C512_PAGE; i++)
    {
        eeprom.parent.write(&eeprom.parent, (uint8_t)(i * 7 + 33));
    }
    eeprom.parent.stop(&eeprom.parent);
    observe_write(1, 1);
    EM_ASM({ Module.writesCompleted(); });

    // Returning from main, deinit, or closing a device could hide the bug by
    // flushing libc. Stay alive until the test runner kills this browser.
    for (;;)
    {
        emscripten_sleep(OBSERVATION_SLEEP_MS);
    }
}
