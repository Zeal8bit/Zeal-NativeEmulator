/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml.hpp>
#include "common/toml_config.h"

namespace zui::common::toml_util {

[[noreturn]] inline void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

inline const toml::table& table_or_fail(const toml::node& node, const std::string& message)
{
    if (const toml::table* table = node.as_table()) {
        return *table;
    }
    fail(message);
}

inline const toml::array& array_or_fail(toml::node_view<const toml::node> node, const std::string& message)
{
    if (const toml::array* array = node.as_array()) {
        return *array;
    }
    fail(message);
}

inline void check_fields(const toml::table& table,
                         std::initializer_list<std::string_view> allowed,
                         const std::string& what)
{
    for (const auto& [key, value] : table) {
        if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end()) {
            fail(what + ": unknown field " + std::string(key.str()));
        }
    }
}

template<typename T>
T value_or(const toml::node& node, const char* key, T fallback, const std::string& what)
{
    auto view = toml::node_view{node}[key];
    if (!view) {
        return fallback;
    }
    if (auto value = view.value_exact<T>()) {
        return *value;
    }
    fail(what + ": invalid field " + key);
}

/* Path relative to the project directory, or the absolute path when outside. */
inline std::string project_relative(const std::filesystem::path& path)
{
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    const auto relative = absolute.lexically_relative(project_directory());
    return (relative.empty() ? absolute : relative).generic_string();
}

} // namespace zui::common::toml_util
