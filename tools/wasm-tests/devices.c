/* Browser persistence proof using the actual EEPROM, SPI and flash devices. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include "hw/i2c/at24c512.h"
#include "hw/flash.h"
#include "utils/config.h"
/* Exercise the card protocol without the display/CPU or a test-only production API. */
#include "../../hw/zvb/zvb_spi.c"
config_t config;
static flash_t flash;

static void seed(const char* path, size_t size, int value)
{
    FILE* file = fopen(path, "rb");
    if (file) { fclose(file); return; }
    file = fopen(path, "wb");
    assert(file);
    void* data = malloc(size);
    assert(data);
    memset(data, value, size);
    assert(fwrite(data, 1, size, file) == size);
    free(data);
    assert(fclose(file) == 0);
}

int main(void)
{
#ifdef DEVICE_MEMORY
    assert(mkdir("/opfs", 0777) == 0);
#else
    assert(wasmfs_create_directory("/opfs", 0777, wasmfs_create_opfs_backend()) == 0);
#endif
    mkdir("/opfs/zeal-device-test", 0700);
    seed("/opfs/zeal-device-test/eeprom.img", 65536, 0);
    seed("/opfs/zeal-device-test/tf.img", 2 * 1024 * 1024, 0);
    seed("/opfs/zeal-device-test/default.img", NOR_FLASH_SIZE_KB, 255);
    static at24c512_t eeprom;
    assert(at24c512_init(&eeprom, "/opfs/zeal-device-test/eeprom.img") == 0);
    const int previous = eeprom.data[123];
    eeprom.parent.start(&eeprom.parent);
    eeprom.parent.write(&eeprom.parent, 0);
    eeprom.parent.write(&eeprom.parent, 123);
    eeprom.parent.write(&eeprom.parent, 0x5a);
    eeprom.parent.stop(&eeprom.parent);
    at24c512_deinit(&eeprom);
    assert(at24c512_init(&eeprom, "/opfs/zeal-device-test/eeprom.img") == 0);
    assert(eeprom.data[123] == 0x5a);
    at24c512_deinit(&eeprom);

    static zvb_spi_t spi;
    zvb_spi_init(&spi);
    assert(zvb_spi_load_tf_image(&spi, "/opfs/zeal-device-test/tf.img") == 0);
    const double started = emscripten_get_now();
    spi.tf_cs = 1;
    for (int block = 0; block < 32; block++) {
        spi.tf.state = TF_WRITE_BLOCK_WAIT_TOK;
        // The card has accepted CMD24; exercise the actual block receiver.
        assert(fseek(spi.tf.img, 512, SEEK_SET) == 0);
        zvb_tf_receive_byte(&spi, 0xfe);
        for (int i = 0; i < 512; i++) zvb_tf_receive_byte(&spi, (uint8_t)i);
        zvb_tf_receive_byte(&spi, 0);
        zvb_tf_receive_byte(&spi, 0);
        assert(spi.tf.reply[0] == TF_WRITE_ACCEPTED);
    }
    assert(fflush(spi.tf.img) == 0);
    assert(fsync(fileno(spi.tf.img)) == 0);
    const double elapsed = emscripten_get_now() - started;
    assert(fseek(spi.tf.img, 512, SEEK_SET) == 0);
    for (int i = 0; i < 512; i++) assert(fgetc(spi.tf.img) == (uint8_t)i);
    assert(fclose(spi.tf.img) == 0);

    assert(flash_init(&flash) == 0);
    assert(flash_load_from_file(&flash, "/opfs/zeal-device-test/default.img", NULL) == 0);
    flash.parent.mem_region.write(&flash.parent, 0x5555, 0xaa);
    flash.parent.mem_region.write(&flash.parent, 0x2aaa, 0x55);
    flash.parent.mem_region.write(&flash.parent, 0x5555, 0xa0);
    flash.parent.mem_region.write(&flash.parent, 321, 0x5a);
    assert(flash_save_to_file(&flash, "/opfs/zeal-device-test/default.img") == 0);
    assert(flash.dirty == 0);
    assert(flash_load_from_file(&flash, "/opfs/zeal-device-test/default.img", NULL) == 0);
    assert(flash.data[321] == 0x5a);
    EM_ASM({document.body.textContent = 'PASS: real EEPROM/TF/NOR persistence; previous EEPROM byte=' + $0 + '; 32 TF sector writes: ' + $1.toFixed(1) + ' ms';}, previous, elapsed);
    return 0;
}
