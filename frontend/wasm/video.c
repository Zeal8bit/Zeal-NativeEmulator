/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "video.h"
#include "hw/zvb/blitter/blitter.h"
#include "utils/log.h"
#include <string.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include "utils/notif.h"

SDL_Window* wasm_window;
SDL_Renderer* wasm_renderer;
static SDL_Texture* texture;

EM_JS(char*, canvas_selector, (), {
    const canvas = Module.canvas;
    if (!canvas.id) {
        canvas.id = 'zeal-' + crypto.randomUUID();
    }
    return stringToNewUTF8('#' + CSS.escape(canvas.id));
});
EM_JS_DEPS(canvas_selector, "$stringToNewUTF8");
#if CONFIG_PROFILE_RENDER
typedef struct {
    double window_start;
    double frame_total;
    double frame_max;
    double zvb_total;
    double zvb_max;
    uint64_t frames;
    uint64_t zvb_calls;
} render_profile_t;

static render_profile_t s_render_profile;
static double last_core_seconds;

static double profile_time(void* userdata)
{
    (void) userdata;
    return (emscripten_get_now() / 1000.0);
}
static void profile_render(void* userdata, double start)
{
    (void) userdata;
    const double elapsed = (emscripten_get_now() / 1000.0) - start;
    last_core_seconds = elapsed;
    s_render_profile.zvb_total += elapsed;
    if (elapsed > s_render_profile.zvb_max) {
        s_render_profile.zvb_max = elapsed;
    }
    s_render_profile.zvb_calls++;
}

static void profile_frame(double elapsed_seconds)
{
    const double now = (emscripten_get_now() / 1000.0);
    if (s_render_profile.window_start == 0.0) {
        s_render_profile.window_start = now;
    }
    elapsed_seconds += last_core_seconds;
    s_render_profile.frame_total += elapsed_seconds;
    if (elapsed_seconds > s_render_profile.frame_max) {
        s_render_profile.frame_max = elapsed_seconds;
    }
    s_render_profile.frames++;

    if (now - s_render_profile.window_start < 1.0) {
        return;
    }

    const double frame_avg = s_render_profile.frames > 0
                                 ? s_render_profile.frame_total / s_render_profile.frames
                                 : 0.0;
    const double zvb_avg = s_render_profile.zvb_calls > 0
                               ? s_render_profile.zvb_total / s_render_profile.zvb_calls
                               : 0.0;
    log_printf(
        "[PROFILE][ZVB] frames=%llu frame=%.2f/%.2fms zvb=%.2f/%.2fms\n",
        (unsigned long long)s_render_profile.frames,
        frame_avg * 1000.0,
        s_render_profile.frame_max * 1000.0,
        zvb_avg * 1000.0,
        s_render_profile.zvb_max * 1000.0);

    memset(&s_render_profile, 0, sizeof(s_render_profile));
    s_render_profile.window_start = now;
}
#endif

static bool canvas_blur(int type, const EmscriptenFocusEvent* event, void* userdata)
{
    (void) type;
    (void) event;
    (void) userdata;
    SDL_ResetKeyboard();
    return false;
}

bool wasm_video_init(zvb_t* zvb)
{
    char* selector = canvas_selector();
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, selector);
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, selector);
    free(selector);
    /* The browser frame callback yields after presenting. */
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
    wasm_window = SDL_CreateWindow(NULL, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT, 0);
    if (wasm_window == NULL) {
        return false;
    }
    emscripten_set_blur_callback(SDL_GetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR), NULL, false, canvas_blur);
    wasm_renderer = SDL_CreateRenderer(wasm_window, NULL);
    if (wasm_renderer == NULL) {
        return false;
    }
    if (!SDL_SetRenderVSync(wasm_renderer, 1)) {
        return false;
    }
    if (!SDL_SetRenderLogicalPresentation(wasm_renderer, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        return false;
    }
    texture = SDL_CreateTexture(wasm_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);
#if CONFIG_PROFILE_RENDER
    zvb->profile_time = profile_time;
    zvb->profile_render = profile_render;
#else
    (void) zvb;
#endif
    return texture && SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
}

bool wasm_video_draw(zvb_t* zvb, bool show_fps, bool preview)
{
    static Uint64 fps_start;
    static unsigned frames, fps;
    const Uint64 now = SDL_GetTicks();
    frames++;
    if (now - fps_start >= 1000) {
        fps = frames * 1000 / (now - fps_start);
        frames = 0;
        fps_start = now;
    }
#if CONFIG_PROFILE_RENDER
    double start = emscripten_get_now();
#endif
    int width, height;
    if (preview) {
        void* pixels;
        int pitch;
        if (!SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
            return false;
        }
        zvb_blitter_preview(zvb, pixels, pitch / sizeof(uint16_t), &width, &height);
        SDL_UnlockTexture(texture);
    } else {
        const zvb_frame_t frame = zvb_frame(zvb);
        width = frame.width;
        height = frame.height;
        const SDL_Rect region = {0, 0, width, height};
        if (!SDL_UpdateTexture(texture, &region, frame.pixels, frame.stride_bytes)) {
            return false;
        }
    }
    const SDL_FRect src = {0, 0, width, height};
    if (!SDL_SetRenderDrawColor(wasm_renderer, 0, 0, 0, 255) || !SDL_RenderClear(wasm_renderer) ||
        !SDL_RenderTexture(wasm_renderer, texture, &src, NULL)) {
        return false;
    }
    notif_render(ZVB_MAX_RES_WIDTH - notif_estimate_width() - 10, 10);
    if (show_fps) {
        SDL_SetRenderDrawColor(wasm_renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(wasm_renderer, 10, 10, "%u FPS", fps);
    }
#if CONFIG_PROFILE_RENDER
    profile_frame((emscripten_get_now() - start) / 1000.0);
#endif
    return SDL_RenderPresent(wasm_renderer);
}

void wasm_video_deinit(void)
{
    if (wasm_window) {
        emscripten_set_blur_callback(SDL_GetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR), NULL, false, NULL);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(wasm_renderer);
    SDL_DestroyWindow(wasm_window);
    texture = NULL;
    wasm_renderer = NULL;
    wasm_window = NULL;
}
