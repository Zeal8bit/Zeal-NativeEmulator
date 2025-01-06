#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *array;
    int rd;
    int wr;
    bool empty;
    size_t size;
} fifo_t;


bool fifo_init(fifo_t *fifo, size_t size);

void fifo_deinit(fifo_t *fifo);

bool fifo_push(fifo_t *fifo, uint8_t value);

bool fifo_pop(fifo_t *fifo, uint8_t *value);

size_t fifo_size(fifo_t *fifo);
