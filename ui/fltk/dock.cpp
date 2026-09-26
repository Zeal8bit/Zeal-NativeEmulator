// SPDX-License-Identifier: Apache-2.0
#include "dock.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <functional>
#include <cmath>
#include <map>
#include <cstdio>
namespace zeal_ui {
static std::unique_ptr<DockNode> tabs(std::initializer_list<int> ids) {
    auto n=std::make_unique<DockNode>(); n->tabs=ids; return n;
}
static std::unique_ptr<DockNode> split(int axis,double ratio,std::unique_ptr<DockNode> a,std::unique_ptr<DockNode> b) {
    auto n=std::make_unique<DockNode>(); n->axis=axis;n->ratio=ratio;n->first=std::move(a);n->second=std::move(b);return n;
}
Workspace::Workspace() { reset(); }
void Workspace::reset() {
    hidden=128; floating.clear();
    root=split(2,.64,split(1,.48,tabs({0}),split(1,.46,split(2,.65,tabs({1}),tabs({2})),tabs({3}))),
        split(1,.55,tabs({4}),split(1,.46,tabs({5}),tabs({6}))));
}
static DockNode* find(DockNode* n,int id) {
    if(!n) return nullptr;
    if(std::find(n->tabs.begin(),n->tabs.end(),id)!=n->tabs.end()) return n;
    auto p=find(n->first.get(),id); return p?p:find(n->second.get(),id);
}
DockNode* Workspace::leaf(int id) const { return find(root.get(),id); }
static bool erase(std::unique_ptr<DockNode>& n,int id) {
    if(!n) return false;
    bool found=false;
    if(n->axis) {
        found=erase(n->first,id); found=erase(n->second,id)||found;
        if(!n->first) n=std::move(n->second);
        else if(!n->second) n=std::move(n->first);
    } else {
        auto i=std::find(n->tabs.begin(),n->tabs.end(),id);
        if(i!=n->tabs.end()) { n->tabs.erase(i); found=true; }
        if(n->tabs.empty()) n.reset();
        else n->selected=std::min(n->selected,(int)n->tabs.size()-1);
    }
    return found;
}
bool Workspace::remove(int id) {
    bool found=erase(root,id);
    auto size=floating.size();
    floating.erase(std::remove_if(floating.begin(),floating.end(),[id](auto f){return f.panel==id;}),floating.end());
    return found || size!=floating.size();
}
void Workspace::dock(int id,int target,int edge) {
    if(id<0||id>7||id==target||edge<0||edge>4) return;
    remove(id); hidden&=~(1u<<id);
    auto n=leaf(target);
    if(!n) { if(!root) root=tabs({id}); else { n=root.get(); while(n->axis) n=n->first.get(); n->tabs.push_back(id); n->selected=n->tabs.size()-1; } return; }
    if(!edge) { n->tabs.push_back(id); n->selected=n->tabs.size()-1; return; }
    auto old=std::make_unique<DockNode>(std::move(*n));
    auto added=tabs({id});
    auto replacement=split(edge<3?1:2,.5,edge==1||edge==3?std::move(added):std::move(old),
        edge==1||edge==3?std::move(old):std::move(added));
    *n=std::move(*replacement);
}
void Workspace::detach(int id,int x,int y) { if(id<0||id>7)return; remove(id);hidden&=~(1u<<id);floating.push_back({id,x,y,640,480}); }
void Workspace::hide(int id) { if(id<0||id>7)return;remove(id);hidden|=1u<<id; }
void Workspace::show(int id) { if(!(hidden&(1u<<id)))return;dock(id,-1,0); }
bool Workspace::save(const std::string& path) const {
    std::ofstream f(path+".tmp"); if(!f)return false;
    f<<"version=1\nhidden="<<hidden<<"\n";
    std::function<void(const DockNode*,std::string)> write=[&](auto n,auto key) {
        if(!n)return;
        f<<key<<"="<<n->axis<<","<<n->ratio<<","<<n->selected;
        for(int id:n->tabs)f<<","<<id;
        f<<"\n";write(n->first.get(),key+"a");write(n->second.get(),key+"b");
    };write(root.get(),"root");
    for(auto w:floating) f<<"float"<<w.panel<<"="<<w.x<<","<<w.y<<","<<w.w<<","<<w.h<<"\n";
    f.close();if(!f)return false;
    return std::rename((path+".tmp").c_str(),path.c_str())==0;
}
bool Workspace::load(const std::string& path) {
    std::ifstream f(path);if(!f)return false;
    Workspace candidate;candidate.root.reset();candidate.floating.clear();
    std::map<std::string,std::string> fields;std::string line;
    while(std::getline(f,line)) { if(line.empty())continue;auto eq=line.find('=');if(eq==line.npos||line.size()>256||!fields.emplace(line.substr(0,eq),line.substr(eq+1)).second)return false; }
    try {
        if(fields.at("version")!="1")return false;
        candidate.hidden=std::stoul(fields.at("hidden"));if(candidate.hidden>255)return false;
        unsigned used=candidate.hidden;size_t consumed=2;
        auto numbers=[](std::string s){std::replace(s.begin(),s.end(),',',' ');return std::istringstream(s);};
        std::function<std::unique_ptr<DockNode>(std::string,int)> read=[&](std::string key,int depth)->std::unique_ptr<DockNode> {
            if(depth>16 || !fields.count(key))throw 1;
            ++consumed;auto n=std::make_unique<DockNode>();auto in=numbers(fields.at(key));
            if(!(in>>n->axis>>n->ratio>>n->selected)||!std::isfinite(n->ratio)||n->ratio<.1||n->ratio>.9)throw 1;
            if(n->axis==0) {
                int id;while(in>>id) { if(id<0||id>7||(used&(1u<<id)))throw 1;used|=1u<<id;n->tabs.push_back(id); }
                if(!in.eof()||n->tabs.empty()||n->selected<0||n->selected>=(int)n->tabs.size())throw 1;
            } else { if(n->axis!=1&&n->axis!=2)throw 1;std::string extra;if(in>>extra)throw 1;n->first=read(key+"a",depth+1);n->second=read(key+"b",depth+1); }
            return n;
        };
        if(fields.count("root"))candidate.root=read("root",0);
        for(int id=0;id<8;++id) { auto key="float"+std::to_string(id);if(!fields.count(key))continue;++consumed;
            if(used&(1u<<id))return false;used|=1u<<id;Floating w{id};auto in=numbers(fields.at(key));std::string extra;
            if(!(in>>w.x>>w.y>>w.w>>w.h)||in>>extra||w.w<160||w.h<100||w.w>16384||w.h>16384)return false;
            candidate.floating.push_back(w);
        }
        if(used!=255||consumed!=fields.size())return false;
    } catch(...) {return false;}
    *this=std::move(candidate);return true;
}
}
