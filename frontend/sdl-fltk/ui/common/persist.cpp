/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/persist.h"
#include <utility>
#include <vector>

namespace zui::common {
static std::vector<std::function<void()>> callbacks;

void persist_register(std::function<void()> capture)
{
    callbacks.push_back(std::move(capture));
}

void persist_capture()
{
    for (auto& capture : callbacks) capture();
}

void persist_finalize()
{
    persist_capture();
    callbacks.clear();
}

} // namespace zui::common
