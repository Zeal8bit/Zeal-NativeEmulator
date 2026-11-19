/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */

// Exercise the frontend's parser and writer without opening a display.
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include "common/machine.h"
#include "common/persist.h"
#include "debugger/watch_model.h"

namespace fs = std::filesystem;

static std::string contents(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

static void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void round_trip(const fs::path& dir)
{
    auto user = (dir / "user.toml").string();
    auto project = (dir / "project.toml").string();
    config.arguments.config_path = user.c_str();
    config.arguments.project_path = project.c_str();
    std::ofstream(user) << "[audio]\nvolume = 80\n[appearance]\ntheme = 'dark'\n";
    std::ofstream(project) << "# Keep this comment and hexadecimal spelling.\n"
        "[audio]\nvolume = 20\n[debugger]\nbreakpoints = [{address=0x461D, enabled=false}]\n"
        "watches = [{expression='message', type='string', length=32, break_on='write', break_enabled=false}]\n";
    const auto original = contents(project);
    check(config_parse_file(user.c_str()) == 0, "valid TOML rejected");
    const auto& watches = zui::debugger::watches();
    check(watches.size() == 1 && watches[0].len == 32 && watches[0].type == zui::debugger::W_STR &&
          watches[0].brk == WATCHPOINT_WRITE && !watches[0].brk_on, "watch fields lost");
    check(config.audio.volume == 20 && config.window.theme == THEME_DARK, "incorrect preference scope");
    config.window.theme = THEME_SYSTEM;
    check(zui::common::config_save_user() == 0 && config_save() == 0, "save failed");
    check(contents(project) == original, "unchanged project was rewritten");
    check(contents(user).find("theme") == std::string::npos, "System override still present");
    config_unload();

    std::ofstream(project) << "[debugger]\nbreakpoints = [{address = 0x10000}]\n";
    const auto invalid = contents(project);
    check(config_parse_file(user.c_str()) != 0, "out-of-range breakpoint accepted");
    check(contents(project) == invalid, "invalid project overwritten");
    config_unload();
    fs::remove(project);
    check(config_parse_file(user.c_str()) == 0 && config.audio.volume == 80, "user volume was overwritten");
    check(config_save() == 0 && !fs::exists(project), "empty project created");
    config_unload();
}

int main()
{
    fs::path dir;
    int result = 0;
    try {
        for (int i = 0;; i++) {
            const auto candidate = fs::temp_directory_path() / ("zeal-fltk-config-" + std::to_string(i));
            if (fs::create_directory(candidate)) {
                dir = candidate;
                break;
            }
        }
        round_trip(dir);
    } catch (const std::exception& ex) {
        fprintf(stderr, "%s\n", ex.what());
        result = 1;
    }
    if (!dir.empty()) fs::remove_all(dir);
    return result;
}
