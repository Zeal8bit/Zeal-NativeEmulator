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

/* Values the callbacks produced when the UI closed (persist_finalize) */
bool s_final;
rini_config s_final_user;
rini_config s_final_project;

void copy_values(const rini_config& from, rini_config* to)
{
    for (unsigned i = 0; i < from.count; i++) {
        const rini_config_value& v = from.values[i];
        if (v.key[0] == '\0') rini_set_config_comment_line(to, v.desc[0] ? v.desc : nullptr);
        else rini_set_config_value_text(to, v.key, v.text, v.desc);
    }
}

void persist_replay(rini_config* user, rini_config* project)
{
    copy_values(s_final_user, user);
    copy_values(s_final_project, project);
}

void drop_final()
{
    if (!s_final) return;
    rini_unload_config(&s_final_user);
    rini_unload_config(&s_final_project);
    s_final = false;
}

} // namespace

void persist_register(std::function<void(rini_config*)> save)
{
    drop_final();
    user_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

void persist_register_project(std::function<void(rini_config*)> save)
{
    drop_final();
    project_list().push_back(std::move(save));
    config_save_hook = persist_save_all;
}

void persist_finalize()
{
    if (user_list().empty() && project_list().empty()) return;
    drop_final();
    s_final_user = rini_load_config(nullptr);
    s_final_project = rini_load_config(nullptr);
    persist_save_all(&s_final_user, &s_final_project);
    s_final = true;
    /* The owners of these callbacks are about to be destroyed */
    user_list().clear();
    project_list().clear();
    config_save_hook = persist_replay;
}

} // namespace zui::common
