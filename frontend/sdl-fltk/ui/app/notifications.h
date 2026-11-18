/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Short notifications (utils/notif.h): shown in the status bar and over the
 * screen, then cleared after a moment. */
#pragma once

namespace zui::app {

/** Text of the notification on show, nullptr when none. */
const char* notif_current_text();
/** Stop the expiry timer (the UI is closing). */
void notif_cancel();

} // namespace zui::app
