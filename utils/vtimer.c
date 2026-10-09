/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>
#include <stddef.h>

#include "utils/vtimer.h"
#include "utils/helpers.h"


/* -------------------------------------------------------------------------- */
/*  Static state                                                              */
/* -------------------------------------------------------------------------- */

/** Sorted singly-linked list head (earliest deadline first). */
static vtimer_node_t* s_head;

/** Monotonically increasing time in nanoseconds. */
static uint64_t s_ns;
static uint64_t s_stall_deadline;


/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

void vtimer_init(void)
{
    s_head  = NULL;
    s_ns    = 0;
    s_stall_deadline = UINT64_MAX;
}

uint64_t vtimer_now_ns(void)
{
    return s_ns;
}

uint64_t vtimer_next_deadline_ns(void)
{
    const uint64_t deadline = s_head != NULL ? s_head->deadline : UINT64_MAX;
    return deadline < s_stall_deadline ? deadline : s_stall_deadline;
}


void vtimer_init_node(vtimer_node_t* node,
                      void (*callback)(void*),
                      void* userdata)
{
    node->callback = callback;
    node->userdata = userdata;
    node->next     = NULL;
}


void vtimer_schedule_at_ns(vtimer_node_t* node, uint64_t deadline_ns)
{
    node->deadline = deadline_ns;
    node->next     = NULL;

    /* Insert sorted by deadline (ascending), stable (FIFO for same deadline). */
    if (s_head == NULL || node->deadline < s_head->deadline) {
        node->next = s_head;
        s_head     = node;
    } else {
        vtimer_node_t* cur = s_head;
        while (cur->next != NULL && cur->next->deadline <= node->deadline) {
            cur = cur->next;
        }
        node->next = cur->next;
        cur->next  = node;
    }
}


void vtimer_schedule_ns(vtimer_node_t* node, uint64_t delay_ns)
{
    vtimer_schedule_at_ns(node, s_ns + delay_ns);
}


void vtimer_schedule_tstates(vtimer_node_t* node, uint64_t delay_tstates)
{
    vtimer_schedule_ns(node, delay_tstates * (1000000000UL / CPUFREQ));
}


void vtimer_schedule_us(vtimer_node_t* node, uint64_t delay_us)
{
    vtimer_schedule_ns(node, delay_us * 1000);
}


static void vtimer_tick_ns(uint64_t elapsed_ns)
{
    s_ns += elapsed_ns;
    while (s_head != NULL && s_head->deadline <= s_ns) {
        vtimer_node_t* node = s_head;
        s_head = node->next;
        if (node->callback != NULL) {
            node->callback(node->userdata);
        }
    }
}


void vtimer_tick(uint64_t elapsed_tstates)
{
    vtimer_tick_ns(elapsed_tstates * (1000000000UL / CPUFREQ));
}


static uint64_t vtimer_stall_until(uint64_t elapsed_tstates, const bool* bus_requested)
{
    /* Dispatch DMA's 20 ns bus events and peripherals in deadline order. */
    const uint64_t start = s_ns;
    const uint64_t tstate_ns = 1000000000UL / CPUFREQ;
    uint64_t target = start + elapsed_tstates * tstate_ns;
    s_stall_deadline = target;
    while (s_head != NULL && s_head->deadline <= target &&
           (bus_requested == NULL || *bus_requested)) {
        vtimer_tick_ns(s_head->deadline - s_ns);
    }
    if (bus_requested != NULL && !*bus_requested) {
        const uint64_t elapsed = s_ns - start;
        target = start + (elapsed / tstate_ns + (elapsed % tstate_ns != 0)) * tstate_ns;
        s_stall_deadline = target;
    }
    vtimer_tick_ns(target - s_ns);
    s_stall_deadline = UINT64_MAX;
    return (s_ns - start) / tstate_ns;
}

void vtimer_stall(uint64_t elapsed_tstates)
{
    vtimer_stall_until(elapsed_tstates, NULL);
}

uint64_t vtimer_stall_bus(uint64_t max_tstates, const bool* bus_requested)
{
    return vtimer_stall_until(max_tstates, bus_requested);
}


void vtimer_cancel(vtimer_node_t* node)
{
    if (node == NULL || s_head == NULL) {
        return;
    }

    /* Special case: head of the list */
    if (s_head == node) {
        s_head = node->next;
        return;
    }

    /* Walk the list to find the node before the target */
    vtimer_node_t* cur = s_head;
    while (cur->next != NULL && cur->next != node) {
        cur = cur->next;
    }

    if (cur->next == node) {
        cur->next = node->next;
    }
}
