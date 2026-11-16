/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/common/app.h"
#include "frontend/headless/console.h"
#include "utils/config.h"
#include "utils/log.h"

void frontend_open(bool debug)
{
    (void) debug;
}
void frontend_reset_input(zeal_t* machine)
{
    (void) machine;
}
static void headless_snes_sample(void* userdata, uint32_t* packets, bool* attached)
{
    (void) userdata;
    attached[0] = false;
    attached[1] = true;
    packets[1] = snes_mouse_packet(0, false, 0, false, SNES_MOUSE_SPEED_SLOW, false, false);
}

static void headless_snes_trace(void* userdata, bool latch, const uint32_t* packets)
{
    (void) userdata;
    v_log_printf(3, latch ? "[SNES] PIO latch port1=none/-1/%04x port2=mouse/-1/%04x\n" : "[SNES] PIO clock port1=none/-1/%08x port2=mouse/-1/%08x\n", packets[0], packets[1]);
}

void frontend_bind_input(zeal_t* machine)
{
    /* Raylib previously flushed earlier startup diagnostics while loading mappings. */
    fflush(stdout);
    machine->snes_adapter.attached[1] = true;
    machine->snes_adapter.sample = headless_snes_sample;
    machine->snes_adapter.trace = headless_snes_trace;
    log_printf("[SNES] Attached mouse to port 1\n");
}
int frontend_bind_output(zeal_t* machine)
{
    (void) machine;
    return 0;
}
void frontend_close(zeal_t* machine)
{
    (void) machine;
}

int frontend_run(zeal_t* machine)
{
    if (config.arguments.console) {
        console_run(machine);
    } else {
        zeal_run(machine, config.arguments.headless_run_ticks);
        if (config.arguments.headless_run_ticks > 0 && machine->cpu.cyc >= config.arguments.headless_run_ticks) {
            log_printf("[ZEAL] Ran for %lu ticks\n", machine->cpu.cyc);
        }
    }
    return machine->compactflash.failed ? 1 : 0;
}
