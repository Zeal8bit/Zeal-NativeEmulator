#include <stdio.h>
#include <stdint.h>
#include <emscripten.h>
#include "utils/files.h"


int files_import(files_options_t* opts)
{
    if (opts == NULL) {
        return 1;
    }
    extern uint8_t* files_import_js(uint32_t* size);
    /* The pointer must be freed with */
    opts->buffer = files_import_js(&opts->size);
    printf("File:\n");
    uint8_t* bytes = (uint8_t*) opts->buffer;
    for(int i = 0; i < 64; i++) {
        printf("%x, ", bytes[i]);
    }
    printf("\n");
    return 0;
}


void files_import_free(files_options_t* opts)
{
    if (opts) {
        free(opts->buffer);
    }
}
