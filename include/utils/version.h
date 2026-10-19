/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Meson defines ZEAL_NATIVE_VERSION from the project version in meson.build.
 * The fallback keeps non-Meson builds (e.g. the Makefile) working; keep it in
 * sync with meson.build when bumping the release version.
 */
#ifndef ZEAL_NATIVE_VERSION
#define ZEAL_NATIVE_VERSION "1.0.0"
#endif

#define ZEAL_NATIVE_NAME "Zeal Native Emulator"
