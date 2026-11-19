/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frontend/sdl-fltk/config.h"
#include "common/toml_config.h"
#include "common/machine.h"
#if CONFIG_ENABLE_DEBUGGER
#include "debugger/project_state.h"
#endif
#include "common/persist.h"
#include <algorithm>
#include <sstream>
#include <iostream>

extern "C" {
#include "utils/paths.h"
#include "frontend/sdl-fltk/paths.h"
}

fltk_settings_args_t fltk_settings_args = {CONFIG_USER_FILENAME, CONFIG_PROJECT_FILENAME, false};

namespace zui::common {
namespace fs = std::filesystem;

namespace {

struct Document {
    fs::path path;
    toml::table data;
    toml::table saved;
    bool writable = true;
};

Document user, project;
bool shared;
std::string image_paths[3];

Document& program()
{
    return shared ? user : project;
}

toml::table& section(Document& doc, const char* name)
{
    auto [it, inserted] = doc.data.insert(name, toml::table{});
    auto* table = it->second.as_table();
    if (!table) throw std::runtime_error(doc.path.string() + ": expected table [" + name + "]");
    return *table;
}

void load(Document& doc, const char* path)
{
    doc = {};
    doc.path = fs::absolute(path).lexically_normal();
    if (doc.path.extension() == ".ini")
        throw std::runtime_error(doc.path.string() + ": zeal-fltk expects TOML; use a separate TOML file");
    if (!fs::exists(doc.path)) return;
    if (!fs::is_regular_file(doc.path)) throw std::runtime_error(doc.path.string() + ": expected a regular file");
    doc.data = toml::parse_file(doc.path.string());
    for (const char* name : {"window", "audio", "appearance", "debugger", "images", "video_window",
                             "code_window", "hardware_window", "console"}) {
        if (auto* node = doc.data.get(name); node && !node->is_table())
            throw std::runtime_error(doc.path.string() + ": expected table [" + name + "]");
    }
    doc.saved = doc.data;
}

bool write_text(FILE* file, const void* data)
{
    const auto& text = *static_cast<const std::string*>(data);
    return fwrite(text.data(), 1, text.size(), file) == text.size();
}

bool save(Document& doc)
{
    if (!doc.writable) return false;
    if (doc.saved == doc.data) return true;
    std::ostringstream out;
    out << doc.data << '\n';
    const std::string text = out.str();
    if (!config_write_file(doc.path.string().c_str(), write_text, &text)) return false;
    doc.saved = doc.data;
    return true;
}

template<typename T>
T read_preference(const char* group, const char* key, T fallback)
{
    auto& doc = !shared && project.data[group][key] ? project : user;
    auto node = doc.data[group][key];
    if (!node) return fallback;
    if constexpr (std::is_same_v<T, bool>) {
        if (auto v = node.value_exact<bool>()) return *v;
    } else if (node.is_integer()) {
        if (auto v = node.value<int>()) return *v;
    }
    fprintf(stderr, "[CONFIG] %s: invalid %s.%s; using default and preserving file\n",
            doc.path.string().c_str(), group, key);
    doc.writable = false;
    return fallback;
}

template<typename T>
void write_preference(const char* group, const char* key, T value)
{
    if (shared || !project.data[group][key]) section(user, group).insert_or_assign(key, value);
}

void save_theme()
{
    if (config.window.theme == THEME_SYSTEM) {
        if (auto* appearance = user.data["appearance"].as_table()) {
            appearance->erase("theme");
            if (appearance->empty()) user.data.erase("appearance");
        }
    } else {
        section(user, "appearance").insert_or_assign("theme", config.window.theme == THEME_LIGHT ? "light" : "dark");
    }
}

int save_settings(bool theme_only)
{
    try {
        save_theme();
        if (theme_only) return save(user) ? 0 : -1;
        persist_capture();
        auto& w = config.window;
        set_preference("window", "width", w.width);
        set_preference("window", "height", w.height);
        set_preference("window", "x", w.x);
        set_preference("window", "y", w.y);
        set_preference("window", "display", w.display);
        set_preference("audio", "volume", config.audio.volume);
#if CONFIG_ENABLE_DEBUGGER
        auto& d = config.debugger;
        set_preference("debugger", "enabled", fltk_settings_args.save_arguments && d.enabled == DEBUGGER_STATE_ARG
                                               ? true : d.config_enabled);
        set_preference("debugger", "width", d.width);
        set_preference("debugger", "height", d.height);
        set_preference("debugger", "x", d.x);
        set_preference("debugger", "y", d.y);
        set_preference("debugger", "uppercase_hex", d.hex_upper);
#endif
        if (fltk_settings_args.save_arguments) {
            const auto& a = config.arguments;
            const char* paths[] = {a.rom_filename, a.eeprom_filename, a.tf_filename};
            const char* keys[] = {"rom", "eeprom", "tf"};
            for (int i = 0; i < 3; ++i) {
                if (!paths[i]) continue;
                auto absolute = fs::absolute(paths[i]).lexically_normal();
                auto relative = absolute.lexically_relative(project_directory());
                section(program(), "images").insert_or_assign(keys[i], (relative.empty() ? absolute : relative).generic_string());
            }
        }
        bool ok = shared || save(project);
        return save(user) && ok ? 0 : -1;
    } catch (const std::exception& ex) {
        fprintf(stderr, "[CONFIG] %s\n", ex.what());
        return -1;
    }
}
} // namespace

const toml::table& project_config() { return program().data; }
fs::path project_directory() { return program().path.parent_path(); }

void set_project_list(const char* key, toml::array values)
{
    auto& doc = program();
    if (!values.empty()) section(doc, "debugger").insert_or_assign(key, std::move(values));
    else if (auto* table = doc.data["debugger"].as_table()) {
        table->erase(key);
        if (table->empty()) doc.data.erase("debugger");
    }
}

int preference(const char* group, const char* key, int fallback) { return read_preference(group, key, fallback); }
bool preference(const char* group, const char* key, bool fallback) { return read_preference(group, key, fallback); }
void set_preference(const char* group, const char* key, int value) { write_preference(group, key, value); }
void set_preference(const char* group, const char* key, bool value) { write_preference(group, key, value); }

} // namespace zui::common

using namespace zui::common;

extern "C" int config_parse_file(const char* file)
{
    try {
        load(user, file);
        shared = path_same_file(file, fltk_settings_args.project_path) ||
                 fs::weakly_canonical(user.path) == fs::weakly_canonical(fs::absolute(fltk_settings_args.project_path));
        if (!shared) load(project, fltk_settings_args.project_path);
        auto choice = user.data["appearance"]["theme"];
        std::string theme = choice ? choice.value_or(std::string{}) : "system";
        if (theme != "system" && theme != "light" && theme != "dark")
            throw std::runtime_error(user.path.string() + ": appearance.theme must be light, dark or system");
        config.window.theme = theme == "light" ? THEME_LIGHT : theme == "dark" ? THEME_DARK : THEME_SYSTEM;
        config.audio.volume = std::clamp(preference("audio", "volume", 100), 0, 100);
        auto& w = config.window;
        w.width = preference("window", "width", -1);
        w.height = preference("window", "height", -1);
        w.x = preference("window", "x", -1);
        w.y = preference("window", "y", -1);
        w.display = preference("window", "display", -1);
        auto& d = config.debugger;
        d.width = preference("debugger", "width", -1);
        d.height = preference("debugger", "height", -1);
        d.x = preference("debugger", "x", -1);
        d.y = preference("debugger", "y", -1);
        d.hex_upper = preference("debugger", "uppercase_hex", true);
        d.config_enabled = preference("debugger", "enabled", false);
        if (d.enabled != DEBUGGER_STATE_ARG && d.enabled != DEBUGGER_STATE_ARG_DISABLE)
            d.enabled = d.config_enabled ? DEBUGGER_STATE_CONFIG : DEBUGGER_STATE_DISABLED;
        auto& a = config.arguments;
        const char** paths[] = {&a.rom_filename, &a.eeprom_filename, &a.tf_filename};
        const char* keys[] = {"rom", "eeprom", "tf"};
        for (int i = 0; i < 3; ++i) {
            auto& doc = program().data["images"][keys[i]] ? program() : user;
            auto node = doc.data["images"][keys[i]];
            if (*paths[i] || !node) continue;
            auto value = node.value<std::string>();
            if (!value || value->empty() || value->find('\0') != std::string::npos)
                throw std::runtime_error(doc.path.string() + ": invalid images." + keys[i]);
            image_paths[i] = (doc.path.parent_path() / *value).lexically_normal().string();
            *paths[i] = image_paths[i].c_str();
        }
#if CONFIG_ENABLE_DEBUGGER
        try {
            zui::debugger::project_state_load();
        } catch (const std::runtime_error& ex) {
            throw std::runtime_error(program().path.string() + ": " + ex.what());
        }
#endif
        return 0;
    } catch (const toml::parse_error& ex) {
        std::cerr << "[CONFIG] " << ex << '\n';
    } catch (const std::exception& ex) {
        fprintf(stderr, "[CONFIG] %s\n", ex.what());
    }
    return -1;
}

extern "C" int config_save() { return save_settings(false); }
int zui::common::config_save_user() { return save_settings(true); }
extern "C" void config_unload() { user = {}; project = {}; }
