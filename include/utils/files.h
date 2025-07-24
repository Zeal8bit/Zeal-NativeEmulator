#pragma once

#include <stdio.h>

#define IN
#define OUT
#define INOUT

typedef struct {
    /* Name hint, can be NULL */
    IN const char* hint;
    /* Buffer to fill, can be NULL to dynamically allocate it */
    INOUT void* buffer;
    /* Size of the buffer, input and output */
    INOUT uint32_t size;
} files_options_t;


int files_import(files_options_t* opts);

void files_import_free(files_options_t* opts);
