/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <toml.hpp>
#include <filesystem>

namespace zui::common {

const toml::table& project_config();
void set_project_list(const char* key, toml::array values);
std::filesystem::path project_directory();

} // namespace zui::common
