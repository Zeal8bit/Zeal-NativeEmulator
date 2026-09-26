/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "debugger/api.h"
#include "debugger/bindings.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define DBG_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define DBG_EXPORT
#endif
uint32_t debugger_bind(dbg_t *);
void debugger_unbind(dbg_t *);
dbg_t *debugger_resolve(uint32_t);
static inline void dbg_put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static inline void dbg_put64(uint8_t *p, uint64_t v)
{
    for (unsigned i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
