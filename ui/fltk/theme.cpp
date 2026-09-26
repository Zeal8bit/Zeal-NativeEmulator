// SPDX-License-Identifier: Apache-2.0
#include "theme.h"
#include <fstream>
#include <sstream>
#include <set>
namespace zeal_ui {
Theme Theme::preset(bool light) {
    Theme t;
    t.colors = light ? std::map<std::string,unsigned>{
        {"background",0xe8edf2},{"surface",0xffffff},{"text",0x202a35},{"muted",0x637080},
        {"border",0xc2ccd6},{"selection",0xc0dcf4},{"link",0x1266ab},{"breakpoint",0xc63550},
        {"current",0xd5eadc},{"paused",0x906500},{"warning",0xc65318}}
        : std::map<std::string,unsigned>{
        {"background",0x202730},{"surface",0x171d25},{"text",0xdce4ed},{"muted",0x8e9eb0},
        {"border",0x3a4655},{"selection",0x314e6b},{"link",0x73b9ef},{"breakpoint",0xef768b},
        {"current",0x254638},{"paused",0xefc46a},{"warning",0xf19a62}};
    return t;
}
static std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r"); if(a==s.npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r")-a+1);
}
bool Theme::load(const std::string& file, Theme& out, std::string& error) {
    std::ifstream f(file); if(!f) { error="Cannot open theme: "+file; return false; }
    std::map<std::string,std::string> fields; std::string line; int number=0;
    while(std::getline(f,line)) {
        ++number; line=trim(line); if(line.empty() || line[0]=='#' || line[0]==';') continue;
        auto eq=line.find('=');
        if(eq==line.npos || !fields.emplace(trim(line.substr(0,eq)),trim(line.substr(eq+1))).second) {
            error="Invalid or duplicate theme field at line "+std::to_string(number); return false;
        }
    }
    if(fields["version"]!="1" || (fields["base"]!="Dark" && fields["base"]!="Light")) {
        error="Theme requires version=1 and base=Dark or Light"; return false;
    }
    Theme t=preset(fields["base"]=="Light");
    try {
        for(auto& [key,value]:fields) {
            if(key=="version" || key=="base") continue;
            if(t.colors.count(key)) {
                if(value.size()!=7 || value[0]!='#' || value.find_first_not_of("0123456789abcdefABCDEF",1)!=value.npos)
                    throw std::runtime_error(key+" must be #RRGGBB");
                t.colors[key]=std::stoul(value.substr(1),nullptr,16);
            } else if(key=="ui_font" || key=="mono_font") {
                if(value.empty() || value.size()>128) throw std::runtime_error("Invalid font name");
                (key=="ui_font"?t.ui_font:t.mono_font)=value;
            } else {
                int* dest=key=="font_size"?&t.font_size:key=="mono_size"?&t.mono_size:
                    key=="spacing"?&t.spacing:key=="row_height"?&t.row_height:nullptr;
                if(!dest) throw std::runtime_error("Unknown theme token: "+key);
                size_t used; int n=std::stoi(value,&used);
                if(used!=value.size() || n<(key=="spacing"?0:10) || n>48) throw std::runtime_error("Invalid size: "+key);
                *dest=n;
            }
        }
        if(t.row_height<t.font_size+4 || t.row_height<t.mono_size+4) throw std::runtime_error("row_height must fit fonts plus 4 pixels");
    } catch(const std::exception& e) { error=e.what(); return false; }
    out=std::move(t); error.clear(); return true;
}
}
