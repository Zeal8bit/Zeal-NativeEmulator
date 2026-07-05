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

/** Monotonically increasing t-state counter. */
static uint64_t s_ticks;


/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

void vtimer_init(void)
{
    s_head  = NULL;
    s_ticks = 0;
}


void vtimer_init_node(vtimer_node_t* node,
                      void (*callback)(void*),
                      void* userdata)
{
    node->callback = callback;
    node->userdata = userdata;
    node->next     = NULL;
}


void vtimer_schedule_tstates(vtimer_node_t* node, uint64_t delay_tstates)
{
    node->deadline = s_ticks + delay_tstates;
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


void vtimer_schedule_us(vtimer_node_t* node, uint64_t delay_us)
{
    vtimer_schedule_tstates(node, US_TO_TSTATES(delay_us));
}


void vtimer_schedule_ns(vtimer_node_t* node, uint64_t delay_ns)
{
    /* 1 t-state = 100 ns @ 10MHz. Round up so we never fire too early. */
    uint64_t delay_tstates = (delay_ns + 99) / 100;
    vtimer_schedule_tstates(node, delay_tstates);
}


void vtimer_tick(uint64_t elapsed_tstates)
{
    s_ticks += elapsed_tstates;

    /* Fire all callbacks whose deadline has been reached. */
    while (s_head != NULL && s_head->deadline <= s_ticks) {
        vtimer_node_t* node = s_head;
        s_head = node->next;

        if (node->callback != NULL) {
            node->callback(node->userdata);
        }
    }
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
