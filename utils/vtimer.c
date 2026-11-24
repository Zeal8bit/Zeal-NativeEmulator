/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <stddef.h>

#include "utils/vtimer.h"
#include "utils/helpers.h"


void vtimer_init(vtimer_t* clock)
{
    clock->head  = NULL;
    clock->now_ns    = 0;
    clock->stall_deadline = UINT64_MAX;
}

uint64_t vtimer_now_ns(vtimer_t* clock)
{
    return clock->now_ns;
}

uint64_t vtimer_next_deadline_ns(vtimer_t* clock)
{
    const uint64_t deadline = clock->head != NULL ? clock->head->deadline : UINT64_MAX;
    return deadline < clock->stall_deadline ? deadline : clock->stall_deadline;
}


void vtimer_init_node(vtimer_node_t* node, vtimer_t* clock,
                      void (*callback)(void*),
                      void* userdata)
{
    node->clock = clock;
    node->callback = callback;
    node->userdata = userdata;
    node->next     = NULL;
}


void vtimer_schedule_at_ns(vtimer_node_t* node, uint64_t deadline_ns)
{
    vtimer_t* clock = node->clock;
    node->deadline = deadline_ns;
    node->next     = NULL;

    /* Insert sorted by deadline (ascending), stable (FIFO for same deadline). */
    if (clock->head == NULL || node->deadline < clock->head->deadline) {
        node->next = clock->head;
        clock->head     = node;
    } else {
        vtimer_node_t* cur = clock->head;
        while (cur->next != NULL && cur->next->deadline <= node->deadline) {
            cur = cur->next;
        }
        node->next = cur->next;
        cur->next  = node;
    }
}


void vtimer_schedule_ns(vtimer_node_t* node, uint64_t delay_ns)
{
    vtimer_schedule_at_ns(node, node->clock->now_ns + delay_ns);
}


void vtimer_schedule_tstates(vtimer_node_t* node, uint64_t delay_tstates)
{
    vtimer_schedule_ns(node, delay_tstates * (1000000000UL / CPUFREQ));
}


void vtimer_schedule_us(vtimer_node_t* node, uint64_t delay_us)
{
    vtimer_schedule_ns(node, delay_us * 1000);
}


static void vtimer_tick_ns(vtimer_t* clock, uint64_t elapsed_ns)
{
    clock->now_ns += elapsed_ns;
    while (clock->head != NULL && clock->head->deadline <= clock->now_ns) {
        vtimer_node_t* node = clock->head;
        clock->head = node->next;
        if (node->callback != NULL) {
            node->callback(node->userdata);
        }
    }
}


void vtimer_tick(vtimer_t* clock, uint64_t elapsed_tstates)
{
    vtimer_tick_ns(clock, elapsed_tstates * (1000000000UL / CPUFREQ));
}


static uint64_t vtimer_stall_until(vtimer_t* clock, uint64_t elapsed_tstates, const bool* bus_requested)
{
    /* Dispatch DMA's 20 ns bus events and peripherals in deadline order. */
    const uint64_t start = clock->now_ns;
    const uint64_t tstate_ns = 1000000000UL / CPUFREQ;
    uint64_t target = start + elapsed_tstates * tstate_ns;
    clock->stall_deadline = target;
    while (clock->head != NULL && clock->head->deadline <= target &&
           (bus_requested == NULL || *bus_requested)) {
        vtimer_tick_ns(clock, clock->head->deadline - clock->now_ns);
    }
    if (bus_requested != NULL && !*bus_requested) {
        const uint64_t elapsed = clock->now_ns - start;
        target = start + (elapsed / tstate_ns + (elapsed % tstate_ns != 0)) * tstate_ns;
        clock->stall_deadline = target;
    }
    vtimer_tick_ns(clock, target - clock->now_ns);
    clock->stall_deadline = UINT64_MAX;
    return (clock->now_ns - start) / tstate_ns;
}

void vtimer_stall(vtimer_t* clock, uint64_t elapsed_tstates)
{
    vtimer_stall_until(clock, elapsed_tstates, NULL);
}

uint64_t vtimer_stall_bus(vtimer_t* clock, uint64_t max_tstates, const bool* bus_requested)
{
    return vtimer_stall_until(clock, max_tstates, bus_requested);
}


void vtimer_cancel(vtimer_node_t* node)
{
    if (node == NULL || node->clock == NULL) {
        return;
    }
    vtimer_t* clock = node->clock;
    if (clock->head == NULL) {
        return;
    }

    /* Special case: head of the list */
    if (clock->head == node) {
        clock->head = node->next;
        return;
    }

    /* Walk the list to find the node before the target */
    vtimer_node_t* cur = clock->head;
    while (cur->next != NULL && cur->next != node) {
        cur = cur->next;
    }

    if (cur->next == node) {
        cur->next = node->next;
    }
}
