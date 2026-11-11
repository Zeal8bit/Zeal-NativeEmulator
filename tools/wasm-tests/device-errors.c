/* Fault injection is limited to calls made by the application device sources.
 * No backend/runtime implementation is replaced. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils/log.h"

static enum {NONE, SEEK_ERROR, SHORT_WRITE, FLUSH_ERROR} fault;
static FILE* target;
static unsigned seeks, writes, flushes, errors;
static char diagnostic[256];
static int device_seek(FILE* file, long offset, int whence)
{
    if (file == target) {
        seeks++;
        if (fault == SEEK_ERROR) { errno = EIO; return -1; }
    }
    return fseek(file, offset, whence);
}
static size_t device_write(const void* data, size_t size, size_t count, FILE* file)
{
    if (file == target) {
        writes++;
        if (fault == SHORT_WRITE) { errno = EIO; return count - 1; }
    }
    return fwrite(data, size, count, file);
}
static int device_flush(FILE* file)
{
    if (file == target) {
        flushes++;
        if (fault == FLUSH_ERROR) {
            /* Let the real libc flush fail on an unavailable descriptor. */
            assert(close(fileno(file)) == 0);
        }
    }
    return fflush(file);
}
static void device_error(const char* format, ...)
{
    errors++;
    va_list args;
    va_start(args, format);
    vsnprintf(diagnostic, sizeof(diagnostic), format, args);
    va_end(args);
}
#define fseek device_seek
#define fwrite device_write
#define fflush device_flush
#undef log_err_printf
#undef log_perror
#define log_err_printf(...) device_error(__VA_ARGS__)
#define log_perror(...) device_error(__VA_ARGS__)
#include "../../hw/zvb/zvb_spi.c"
#include "../../hw/i2c/at24c512.c"
#undef fseek
#undef fwrite
#undef fflush
config_t config;

static void reset(int failure)
{
    fault = failure;
    seeks = writes = flushes = errors = 0;
    diagnostic[0] = 0;
}

int main(void)
{
    static zvb_spi_t spi;
    for (int failure = NONE; failure <= FLUSH_ERROR; failure++) {
        if (failure == SEEK_ERROR) continue; // Sector positioning precedes completion.
        zvb_spi_init(&spi);
        target = spi.tf.img = tmpfile();
        assert(target);
        spi.tf_cs = 1;
        spi.tf.state = TF_WRITE_BLOCK_WAIT_TOK;
        reset(failure);
        zvb_tf_receive_byte(&spi, 0xfe);
        for (int i = 0; i < 512; i++) zvb_tf_receive_byte(&spi, (uint8_t)i);
        zvb_tf_receive_byte(&spi, 0);
        assert(writes == 0 && flushes == 0); // Incomplete sector must not flush.
        zvb_tf_receive_byte(&spi, 0);
        assert(writes == 1 && flushes == 1);
        assert(spi.tf.reply[0] == (failure == NONE ? TF_WRITE_ACCEPTED : TF_WRITE_ERROR));
        assert(errors == (failure == NONE ? 0 : 1));
        if (failure == FLUSH_ERROR) assert(strstr(diagnostic, "flush"));
        // Response and diagnostics were checked before cleanup could flush data.
        fault = NONE;
        fclose(target);
    }
    static at24c512_t eeprom;
    for (int failure = NONE; failure <= FLUSH_ERROR; failure++) {
        assert(at24c512_init(&eeprom, NULL) == 0);
        target = eeprom.file = tmpfile();
        assert(target);
        reset(failure);
        eeprom.parent.start(&eeprom.parent);
        eeprom.parent.write(&eeprom.parent, 1);
        eeprom.parent.write(&eeprom.parent, 0);
        for (int i = 0; i < (failure == NONE ? 1 : AT24C512_PAGE); i++) eeprom.parent.write(&eeprom.parent, (uint8_t)i);
        assert(seeks == 0 && writes == 0 && flushes == 0); // Only STOP completes the page.
        eeprom.parent.stop(&eeprom.parent);
        assert(!eeprom.writing && seeks == 1);
        assert(writes == (failure == SEEK_ERROR ? 0 : 1));
        assert(flushes == (failure == SEEK_ERROR ? 0 : 1));
        assert(errors == (failure == NONE ? 0 : 1));
        if (failure == SEEK_ERROR) assert(strstr(diagnostic, "seek"));
        if (failure == FLUSH_ERROR) assert(strstr(diagnostic, "flush"));
        fault = NONE;
        fclose(target);
    }
    puts("PASS: TF completion responses and EEPROM diagnostics on seek/write/flush failures; complete-boundary I/O only");
    return 0;
}
