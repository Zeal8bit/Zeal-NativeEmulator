/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zvb/zvb_timer.h"
#include <string.h>

#define FPGA_CLOCK_NS 20u

static uint32_t ticks_to_zero(const zvb_timer_t* timer, uint16_t value)
{
    if (!value) return 65536u;
    return timer->decrement ? value : 65536u - value;
}

static void timer_notify(zvb_timer_t* timer, bool previous_irq)
{
    if (previous_irq != zvb_timer_interrupt(timer) && timer->irq_changed)
        timer->irq_changed(timer->userdata);
}

static void timer_sync(zvb_timer_t* timer, uint64_t now)
{
    const bool previous_irq = zvb_timer_interrupt(timer);
    const uint64_t clocks = (now - timer->clock_time) / FPGA_CLOCK_NS;
    timer->clock_time += clocks * FPGA_CLOCK_NS;
    if (!timer->enabled || !clocks) return;
    const uint32_t first = timer->clock_counter >= timer->divider ? 1u :
                           timer->divider - timer->clock_counter + 1u;
    if (clocks < first) {
        timer->clock_counter += clocks;
        return;
    }
    const uint32_t period = (uint32_t)timer->divider + 1u;
    uint64_t ticks = 1u + (clocks - first) / period;
    timer->clock_counter = (clocks - first) % period;
    const int direction = timer->decrement ? -1 : 1;
    if (timer->counter || !timer->auto_reload) {
        const uint32_t distance = ticks_to_zero(timer, timer->counter);
        if (ticks < distance) {
            timer->counter = timer->counter + direction * (int)ticks;
            return;
        }
        ticks -= distance;
        timer->counter = 0;
        timer->int_pending = true;
        if (!timer->auto_reload) {
            timer->enabled = false;
            timer->clock_counter = 0;
        }
    }
    if (timer->auto_reload) {
        /* RTL reloads on the tick AFTER reaching zero. Skip complete cycles
         * arithmetically instead of scheduling every 50 MHz clock. */
        const uint32_t cycle = timer->reload ? ticks_to_zero(timer, timer->reload) + 1u : 1u;
        if (timer->reload && ticks >= cycle) timer->int_pending = true;
        ticks %= cycle;
        if (ticks) timer->counter = timer->reload + direction * (int)(ticks - 1u);
    }
    timer_notify(timer, previous_irq);
}

static void timer_schedule(zvb_timer_t* timer)
{
    vtimer_cancel(&timer->event);
    if (!timer->enabled || timer->int_pending) return;
    uint32_t ticks;
    if (!timer->counter && timer->auto_reload) {
        if (!timer->reload) return; /* Reloading zero never sets the RTL IRQ. */
        ticks = 1u + ticks_to_zero(timer, timer->reload);
    } else {
        ticks = ticks_to_zero(timer, timer->counter);
    }
    const uint32_t first = timer->clock_counter >= timer->divider ? 1u :
                           timer->divider - timer->clock_counter + 1u;
    const uint64_t clocks = first + (uint64_t)(ticks - 1u) * ((uint32_t)timer->divider + 1u);
    vtimer_schedule_at_ns(&timer->event, timer->clock_time + clocks * FPGA_CLOCK_NS);
}

static void timer_event(void* userdata)
{
    zvb_timer_t* timer = userdata;
    timer_sync(timer, timer->event.deadline);
    timer_schedule(timer);
}

void zvb_timer_init(zvb_timer_t* timer, void (*irq_changed)(void*), void* userdata)
{
    memset(timer, 0, sizeof(*timer));
    timer->irq_changed = irq_changed;
    timer->userdata = userdata;
    vtimer_init_node(&timer->event, timer_event, timer);
    zvb_timer_reset(timer);
}

void zvb_timer_reset(zvb_timer_t* timer)
{
    const bool previous_irq = zvb_timer_interrupt(timer);
    vtimer_cancel(&timer->event);
    timer->divider = timer->clock_counter = timer->reload = 0;
    timer->enabled = timer->decrement = timer->int_enabled = timer->int_pending = false;
    /* RTL reset retains accumulator, auto-reload and byte latches. */
    timer->clock_time = vtimer_now_ns() / FPGA_CLOCK_NS * FPGA_CLOCK_NS;
    timer_notify(timer, previous_irq);
}

uint8_t zvb_timer_read(zvb_timer_t* timer, uint32_t address)
{
    timer_sync(timer, vtimer_now_ns());
    switch (address & 15) {
        case 0: return (timer->enabled << 7) | (timer->auto_reload << 6) |
                       (timer->decrement << 5) | (timer->int_enabled << 4);
        case 1: timer->read_latch = timer->divider >> 8; return timer->divider;
        case 3: timer->read_latch = timer->reload >> 8; return timer->reload;
        case 5: timer->read_latch = timer->counter >> 8; return timer->counter;
        case 2: case 4: case 6: return timer->read_latch;
        case 7: return timer->int_pending;
        default: return 0;
    }
}

void zvb_timer_write(zvb_timer_t* timer, uint32_t address, uint8_t data)
{
    timer_sync(timer, vtimer_now_ns());
    const bool previous_irq = zvb_timer_interrupt(timer);
    const uint16_t word = ((uint16_t)data << 8) | timer->write_latch;
    switch (address & 15) {
        case 0:
            /* Bit 7 starts/restarts the divider; clearing it does not stop RTL. */
            if (data & 0x80) { timer->enabled = true; timer->clock_counter = 0; }
            timer->auto_reload = (data & 0x40) != 0;
            timer->decrement = (data & 0x20) != 0;
            timer->int_enabled = (data & 0x10) != 0;
            if (data & 1) timer->counter = timer->reload;
            break;
        case 1: case 3: case 5: timer->write_latch = data; break;
        case 2: timer->divider = word; break;
        case 4: timer->reload = word; break;
        case 6: timer->counter = word; break;
        case 7: if (data & 1) timer->int_pending = false; break;
        default: break;
    }
    timer_notify(timer, previous_irq);
    timer_schedule(timer);
}
