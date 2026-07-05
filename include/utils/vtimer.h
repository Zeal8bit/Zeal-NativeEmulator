/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file vtimer.h
 * @brief Virtual timer system for scheduling deferred callbacks based on t-state count.
 *
 * Maintains a sorted singly-linked list of timer nodes. Callers schedule a callback
 * with a delay in t-states, microseconds, or nanoseconds. After each Z80 instruction,
 * vtimer_tick() is called with the elapsed t-states to fire any due callbacks.
 *
 * All state is global/static — there is no per-machine instance.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief A node in the timer linked list. Returned by schedule functions as a handle
 *        that can be passed to vtimer_cancel().
 */
typedef struct vtimer_node {
    struct vtimer_node* next;
    uint64_t            deadline;   /* absolute t-state count when this fires   */
    void                (*callback)(void* userdata);
    void*               userdata;
} vtimer_node_t;


/**
 * @brief Initialize the virtual timer subsystem.
 *        Resets the internal t-state counter to 0 and clears the timer list.
 *        Must be called once at program startup.
 */
void vtimer_init(void);


/**
 * @brief Initialize a timer node with its callback and userdata.
 *
 * Sets the callback/userdata once. The node can then be scheduled repeatedly
 * via vtimer_schedule_* without re-specifying them.
 *
 * @param node      Timer node to initialize.
 * @param callback  Function to call when the timer fires.
 * @param userdata  Opaque pointer passed to the callback.
 */
void vtimer_init_node(vtimer_node_t* node,
                      void (*callback)(void*),
                      void* userdata);


/**
 * @brief Schedule a one-shot callback after @p delay_tstates t-states from now.
 *
 * The node must have been initialized with vtimer_init_node() first.
 * The caller owns @p node's memory (typically embedded in a struct or static).
 * The node must remain valid until the callback fires or vtimer_cancel() is called.
 *
 * @param node           Timer node to link into the list.
 * @param delay_tstates  Number of t-states to wait before firing.
 */
void vtimer_schedule_tstates(vtimer_node_t* node, uint64_t delay_tstates);


/**
 * @brief Schedule a one-shot callback after @p delay_us microseconds.
 *
 * Converts microseconds to t-states using the 10 MHz CPU clock (1 us = 10 t-states).
 *
 * @param node      Timer node to link into the list.
 * @param delay_us  Delay in microseconds.
 */
void vtimer_schedule_us(vtimer_node_t* node, uint64_t delay_us);


/**
 * @brief Schedule a one-shot callback after @p delay_ns nanoseconds.
 *
 * Converts nanoseconds to t-states using the 10 MHz CPU clock (1 t-state = 100 ns).
 * Fractional t-states are rounded up to ensure the delay is never shorter than requested.
 *
 * @param node      Timer node to link into the list.
 * @param delay_ns  Delay in nanoseconds.
 */
void vtimer_schedule_ns(vtimer_node_t* node, uint64_t delay_ns);


/**
 * @brief Advance the virtual timer by @p elapsed_tstates t-states.
 *
 * Fires all callbacks whose deadline has been reached (deadline <= current t-states).
 * Each callback is invoked exactly once, then its node is unlinked from the list.
 * The caller is responsible for the node's lifetime (it is not freed).
 *
 * Call this after every Z80 instruction with the number of t-states that instruction took.
 *
 * @param elapsed_tstates  Number of t-states to advance the counter by.
 */
void vtimer_tick(uint64_t elapsed_tstates);


/**
 * @brief Cancel a previously scheduled timer.
 *
 * Unlinks the node from the timer list. If the timer has already fired (or was
 * already cancelled), this is a no-op. The node is not freed — the caller owns it.
 *
 * @param node  Timer node to cancel.
 */
void vtimer_cancel(vtimer_node_t* node);
