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

namespace {

using SaveFn = std::function<void(rini_config*)>;

std::vector<SaveFn>& user_list()
{
    static std::vector<SaveFn> list;
    return list;
}

std::vector<SaveFn>& project_list()
{
    static std::vector<SaveFn> list;
    return list;
}

void persist_save_all(rini_config* user, rini_config* project)
{
    for (auto& save : user_list()) save(user);
    for (auto& save : project_list()) save(project);
}

} // namespace

void persist_register(std::function<void(rini_config*)> save)
{
    user_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

void persist_register_project(std::function<void(rini_config*)> save)
{
    project_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

} // namespace zui::common
