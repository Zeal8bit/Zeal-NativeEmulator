/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>

namespace zui::common {

int preference(const char* section, const char* key, int fallback);
bool preference(const char* section, const char* key, bool fallback);
void set_preference(const char* section, const char* key, int value);
void set_preference(const char* section, const char* key, bool value);
void persist_register(std::function<void()> capture);
int config_save_user();
void persist_capture();
void persist_finalize();

} // namespace zui::common
