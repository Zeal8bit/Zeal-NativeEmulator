/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/* Set the host directory the guest sees as its root; called once by
 * desktop_run(). Returns non-zero if the path cannot be opened. */
int frontend_hostfs_load_path(const char* root_path);
