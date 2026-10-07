/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "utils/vtimer.h"

/* ZealTimer.v: shared byte latches and a 50 MHz divided 16-bit counter. */
typedef struct {
    uint16_t divider, clock_counter, reload, counter;
    uint8_t read_latch, write_latch;
    bool enabled, auto_reload, decrement, int_enabled, int_pending;
    uint64_t clock_time;
    vtimer_node_t event;
    void (*irq_changed)(void*);
    void* userdata;
} zvb_timer_t;

void zvb_timer_init(zvb_timer_t* timer, void (*irq_changed)(void*), void* userdata);
void zvb_timer_reset(zvb_timer_t* timer);
uint8_t zvb_timer_read(zvb_timer_t* timer, uint32_t address);
void zvb_timer_write(zvb_timer_t* timer, uint32_t address, uint8_t data);
static inline bool zvb_timer_interrupt(const zvb_timer_t* timer)
{ return timer->int_enabled && timer->int_pending; }
