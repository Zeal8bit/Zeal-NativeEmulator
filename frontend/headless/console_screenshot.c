/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "frontend/headless/console_screenshot.h"
#include "hw/zvb/zvb.h"
#include "utils/log.h"

/* Frames are only rendered when frame output is enabled, which headless
 * leaves off by default. The first screenshot turns it on and renders the
 * current video state (without mid-frame raster effects); later captures
 * return the last frame completed by normal execution, raster effects
 * included. Run at least one frame of T-states between captures for that. */
void console_screenshot(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: screenshot <file.ppm>\n");
        return;
    }
    zvb_t* zvb = &machine->zvb;
    if (!zvb->rendering_enabled) {
        if (zvb_set_frame_output(zvb, true) != 0) {
            log_err_printf("[CONSOLE] Could not enable frame output\n");
            return;
        }
        zvb_force_render(zvb);
    } else if (zvb_frame(zvb).sequence == 0) {
        zvb_force_render(zvb);
    }

    const zvb_frame_t frame = zvb_frame(zvb);
    if (frame.pixels == NULL || frame.width <= 0 || frame.height <= 0) {
        log_err_printf("[CONSOLE] No frame available\n");
        return;
    }
    FILE* f = fopen(argv[1], "wb");
    if (f == NULL) {
        log_err_printf("[CONSOLE] Cannot open '%s'\n", argv[1]);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", frame.width, frame.height);
    for (int y = 0; y < frame.height; y++) {
        const uint16_t* row = (const uint16_t*) ((const uint8_t*) frame.pixels + y * frame.stride_bytes);
        for (int x = 0; x < frame.width; x++) {
            const uint16_t c = row[x];
            const uint8_t r5 = (c >> 11) & 0x1f, g6 = (c >> 5) & 0x3f, b5 = c & 0x1f;
            const uint8_t rgb[3] = {
                (uint8_t) ((r5 << 3) | (r5 >> 2)),
                (uint8_t) ((g6 << 2) | (g6 >> 4)),
                (uint8_t) ((b5 << 3) | (b5 >> 2)),
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    log_printf("[CONSOLE] Screenshot %dx%d (frame %llu) written to %s\n", frame.width, frame.height,
               (unsigned long long) frame.sequence, argv[1]);
}
