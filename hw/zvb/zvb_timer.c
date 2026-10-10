/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zvb/zvb_timer.h"
#include <string.h>

#define TIMER_REGISTER_MASK              (15)
#define TIMER_COUNTER_RANGE              (65536u)
#define TIMER_CTRL_ENABLE                (0x80)
#define TIMER_CTRL_AUTO_RELOAD           (0x40)
#define TIMER_CTRL_DECREMENT             (0x20)
#define TIMER_CTRL_INT_ENABLE            (0x10)
#define TIMER_CTRL_LOAD                  (1)
#define TIMER_INT_CLEAR                  (1)
#define TIMER_ENABLE_SHIFT               (7)
#define TIMER_AUTO_RELOAD_SHIFT          (6)
#define TIMER_DECREMENT_SHIFT            (5)
#define TIMER_INT_ENABLE_SHIFT           (4)

#define TIMER_CLOCK_NS 20u

static uint32_t ticks_to_zero(const zvb_timer_t* timer, uint16_t value)
{
    if (!value) {
        return TIMER_COUNTER_RANGE;
    }
    return timer->decrement ? value : TIMER_COUNTER_RANGE - value;
}

static void timer_notify(zvb_timer_t* timer, bool previous_irq)
{
    if (previous_irq != zvb_timer_interrupt(timer) && timer->irq_changed) {
        timer->irq_changed(timer->userdata);
    }
}

static void timer_sync(zvb_timer_t* timer, uint64_t now)
{
    const bool previous_irq = zvb_timer_interrupt(timer);
    const uint64_t clocks = (now - timer->clock_time) / TIMER_CLOCK_NS;
    timer->clock_time += clocks * TIMER_CLOCK_NS;
    if (!timer->enabled || !clocks) {
        return;
    }
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
        /* Reload on the tick after reaching zero. Skip complete cycles
         * arithmetically instead of scheduling every 50 MHz clock. */
        const uint32_t cycle = timer->reload ? ticks_to_zero(timer, timer->reload) + 1u : 1u;
        if (timer->reload && ticks >= cycle) {
            timer->int_pending = true;
        }
        ticks %= cycle;
        if (ticks) {
            timer->counter = timer->reload + direction * (int)(ticks - 1u);
        }
    }
    timer_notify(timer, previous_irq);
}

static void timer_schedule(zvb_timer_t* timer)
{
    vtimer_cancel(&timer->event);
    if (!timer->enabled || timer->int_pending) {
        return;
    }
    uint32_t ticks;
    if (!timer->counter && timer->auto_reload) {
        if (!timer->reload) {
            return; /* Reloading zero never sets the interrupt. */
        }
        ticks = 1u + ticks_to_zero(timer, timer->reload);
    } else {
        ticks = ticks_to_zero(timer, timer->counter);
    }
    const uint32_t first = timer->clock_counter >= timer->divider ? 1u :
                           timer->divider - timer->clock_counter + 1u;
    const uint64_t clocks = first + (uint64_t)(ticks - 1u) * ((uint32_t)timer->divider + 1u);
    vtimer_schedule_at_ns(&timer->event, timer->clock_time + clocks * TIMER_CLOCK_NS);
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
    timer->reload = 0;
    timer->clock_counter = 0;
    timer->divider = 0;
    timer->int_pending = false;
    timer->int_enabled = false;
    timer->decrement = false;
    timer->enabled = false;
    /* Reset retains the accumulator, auto-reload setting and byte latches. */
    timer->clock_time = vtimer_now_ns() / TIMER_CLOCK_NS * TIMER_CLOCK_NS;
    timer_notify(timer, previous_irq);
}

uint8_t zvb_timer_read(zvb_timer_t* timer, uint32_t address)
{
    timer_sync(timer, vtimer_now_ns());
    switch (address & TIMER_REGISTER_MASK) {
        case TIMER_REG_CTRL:
            return (timer->enabled << TIMER_ENABLE_SHIFT) | (timer->auto_reload << TIMER_AUTO_RELOAD_SHIFT) |
                       (timer->decrement << TIMER_DECREMENT_SHIFT) | (timer->int_enabled << TIMER_INT_ENABLE_SHIFT);
        case TIMER_REG_DIV_LOW:
            timer->read_latch = timer->divider >> 8;
            return timer->divider;
        case TIMER_REG_RELOAD_LOW:
            timer->read_latch = timer->reload >> 8;
            return timer->reload;
        case TIMER_REG_COUNTER_LOW:
            timer->read_latch = timer->counter >> 8;
            return timer->counter;
        case TIMER_REG_DIV_HIGH:
        case TIMER_REG_RELOAD_HIGH:
        case TIMER_REG_COUNTER_HIGH:
            return timer->read_latch;
        case TIMER_REG_INT_STATUS:
            return timer->int_pending;
        default:
            return 0;
    }
}

void zvb_timer_write(zvb_timer_t* timer, uint32_t address, uint8_t data)
{
    timer_sync(timer, vtimer_now_ns());
    const bool previous_irq = zvb_timer_interrupt(timer);
    const uint16_t word = ((uint16_t)data << 8) | timer->write_latch;
    switch (address & TIMER_REGISTER_MASK) {
        case TIMER_REG_CTRL:
            /* Bit 7 starts/restarts the divider; clearing it does not stop the timer. */
            if (data & TIMER_CTRL_ENABLE) {
                timer->enabled = true;
                timer->clock_counter = 0;
            }
            timer->auto_reload = (data & TIMER_CTRL_AUTO_RELOAD) != 0;
            timer->decrement = (data & TIMER_CTRL_DECREMENT) != 0;
            timer->int_enabled = (data & TIMER_CTRL_INT_ENABLE) != 0;
            if (data & TIMER_CTRL_LOAD) {
                timer->counter = timer->reload;
            }
            break;
        case TIMER_REG_DIV_LOW:
        case TIMER_REG_RELOAD_LOW:
        case TIMER_REG_COUNTER_LOW:
            timer->write_latch = data;
            break;
        case TIMER_REG_DIV_HIGH:
            timer->divider = word;
            break;
        case TIMER_REG_RELOAD_HIGH:
            timer->reload = word;
            break;
        case TIMER_REG_COUNTER_HIGH:
            timer->counter = word;
            break;
        case TIMER_REG_INT_STATUS:
            if (data & TIMER_INT_CLEAR) {
                timer->int_pending = false;
            }
            break;
        default:
            break;
    }
    timer_notify(timer, previous_irq);
    timer_schedule(timer);
}
