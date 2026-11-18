/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/persist.h"
#include <functional>
#include <vector>
#include "common/machine.h"

namespace zui::common {

static std::vector<std::function<void(rini_config*)>>& persist_list()
{
    static std::vector<std::function<void(rini_config*)>> list;
    return list;
}

static void persist_save_all(rini_config* ini)
{
    for (auto& save : persist_list()) save(ini);
}

void persist_register(std::function<void(rini_config*)> save)
{
    persist_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

} // namespace zui::common
