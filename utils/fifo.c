#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utils/fifo.h"


bool fifo_init(fifo_t *fifo, size_t size) {
    if (fifo == NULL || size == 0) {
        return false;
    }

    fifo->array = (uint8_t *)malloc(size * sizeof(uint8_t));
    if (fifo->array == NULL) {
        return false;
    }

    fifo->rd = 0;
    fifo->wr = 0;
    fifo->empty = true;
    fifo->size = size;
    return true;
}


void fifo_deinit(fifo_t *fifo) {
    if (fifo == NULL) {
        return;
    }
    free(fifo->array);
    fifo->array = NULL;
    fifo->rd = 0;
    fifo->wr = 0;
    fifo->empty = true;
    fifo->size = 0;
}


bool fifo_push(fifo_t *fifo, uint8_t value) {
    if (!fifo || !fifo->array) return false;

    /* Check if the FIFO is full */
    if (!fifo->empty && fifo->rd == fifo->wr) {
        return false;
    }

    /* FIFO is not full, add the new element */
    fifo->array[fifo->wr] = value;
    fifo->wr = (fifo->wr + 1) % fifo->size;
    fifo->empty = false;

    return true;
}


// Pop function to retrieve an element from the FIFO.
bool fifo_pop(fifo_t *fifo, uint8_t *value) {
    if (!fifo || !fifo->array || !value) return false;

    /* Make sure the FIFO is not empty */
    if (fifo->empty) {
        return false;
    }

    *value = fifo->array[fifo->rd];
    fifo->rd = (fifo->rd + 1) % fifo->size;

    /* Check if FIFO is now empty after popping an element */
    if (fifo->rd == fifo->wr) {
        fifo->empty = true;
    }

    return true;
}


size_t fifo_size(fifo_t *fifo) {
    if (fifo == NULL || fifo->array == NULL || fifo->empty) {
        return 0;
    }

    if (fifo->wr >= fifo->rd) {
        return fifo->wr - fifo->rd;
    } else {
        return (fifo->size - fifo->rd) + fifo->wr;
    }
}
