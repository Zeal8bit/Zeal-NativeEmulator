// SPDX-License-Identifier: Apache-2.0
#include "debugger/frontend.h"
#include "theme.h"
#include "dock.h"
#include "cp437.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/fl_utf8.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <vector>

using zeal_ui::Theme;
using zeal_ui::Workspace;
using zeal_ui::DockNode;
static const char* names[]={"Video","CPU","Breakpoints","Disassembler","Memory","MMU","Semihost","VRAM"};
struct Panel;
struct DockSurface;
struct Rect { int x,y,w,h; bool contains(int px,int py) const { return px>=x&&py>=y&&px<x+w&&py<y+h; } };
struct dbg_ui_t {
    dbg_ui_init_args_t host;
    Fl_Double_Window* window=nullptr;
    Fl_Menu_Bar* menu=nullptr;
    Fl_Box* status=nullptr;
    DockSurface* surface=nullptr;
    std::array<Panel*,8> panels{};
    std::map<int,Fl_Double_Window*> floating;
    Workspace workspace;
    Theme theme=Theme::preset(false);
    std::string theme_name="Dark",directory;
    Fl_Font ui_font=FL_HELVETICA,mono_font=FL_COURIER;
    bool shown=false,passthrough=false,upper=true,cp437=false,rebuild=false;
    int scale=1,volume=100,drag=-1,drag_x=0,drag_y=0;
    double last_refresh=0;
    dbg_snapshot_t snapshot{};
    std::string message;
    Fl_Color color(const char* role) const { auto rgb=theme.colors.at(role);return fl_rgb_color(rgb>>16,(rgb>>8)&255,rgb&255); }
    void apply_theme();
    void select_theme(const std::string&);
    void build_menu();
    void layout();
    void save();
    void hide_panel(int id);
    void drop(int x,int y);
    void command(dbg_command_t c) { debugger_command(host.debugger,c);last_refresh=0; }
    void release() { host.release_input(host.debugger); }
    void navigate(uint32_t addr);
};
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static uint32_t address(dbg_ui_t* ui,const char* s,bool* valid=nullptr) {
    hwaddr a=0; bool ok=debugger_find_symbol(ui->host.debugger,s,&a);
    if(!ok) { if(*s=='$')++s; char* end=nullptr;unsigned long n=std::strtoul(s,&end,16);ok=*s&&end&&!*end&&n<=0xffff;a=(uint32_t)n; }
    if(valid)*valid=ok;return a;
}
static void text(dbg_ui_t* u,const char* role,const std::string& s,int x,int y,bool mono=true) {
    fl_color(u->color(role));fl_font(mono?u->mono_font:u->ui_font,mono?u->theme.mono_size:u->theme.font_size);fl_draw(s.c_str(),x,y);
}
static std::string hex(unsigned n,int digits,bool upper=true) {
    char b[32];std::snprintf(b,sizeof(b),upper?"%0*X":"%0*x",digits,n);return b;
}
static std::string glyph(uint8_t b,bool cp) {
    if(!cp) return std::string(1,b>=32&&b<127?char(b):'.');
    char out[8]={};int n=fl_utf8encode(s_cp437_to_unicode[b],out);return std::string(out,n);
}
// Host key vocabulary uses stable GLFW-compatible numeric codes, interpreted in C.
// No Raylib headers or implementation types cross the frontend boundary.
static unsigned translate_key(int k) {
    if(k>='a'&&k<='z')return k-'a'+'A';
    if(k>=32&&k<=96)return k;
    if(k>FL_F&&k<=FL_F+12)return 289+k-FL_F;
    if(k>=FL_KP+'0'&&k<=FL_KP+'9')return 320+k-FL_KP-'0';
    switch(k) {
    case FL_Escape:return 256;case FL_Enter:return 257;case FL_Tab:return 258;case FL_BackSpace:return 259;
    case FL_Insert:return 260;case FL_Delete:return 261;case FL_Right:return 262;case FL_Left:return 263;
    case FL_Down:return 264;case FL_Up:return 265;case FL_Page_Up:return 266;case FL_Page_Down:return 267;
    case FL_Home:return 268;case FL_End:return 269;case FL_Caps_Lock:return 280;case FL_Scroll_Lock:return 281;
    case FL_Num_Lock:return 282;case FL_Print:return 283;case FL_Pause:return 284;
    case FL_Shift_L:return 340;case FL_Control_L:return 341;case FL_Alt_L:return 342;case FL_Meta_L:return 343;
    case FL_Shift_R:return 344;case FL_Control_R:return 345;case FL_Alt_R:return 346;case FL_Meta_R:return 347;
    case FL_KP_Enter:return 335;case FL_KP+'.':return 330;case FL_KP+'/':return 331;
    case FL_KP+'*':return 332;case FL_KP+'-':return 333;case FL_KP+'+':return 334;
    default:return 0;
    }
}

struct Canvas:Fl_Widget {
    Panel* panel;
    Canvas(Panel* p,int x,int y,int w,int h):Fl_Widget(x,y,w,h),panel(p){}
    void draw() override;
    int handle(int event) override;
};
struct Panel:Fl_Group {
    dbg_ui_t* ui;int id;
    Fl_Input* location=nullptr;
    Fl_Choice* choice=nullptr;
    Canvas* canvas=nullptr;
    std::array<Fl_Input*,14> registers{};
    regs_t regs{};
    uint32_t start=0,range=256;
    int hover=-1,scroll_x=0,scroll_y=0,mouse_x=0,mouse_y=0;
    bool captured=false;
    std::vector<uint8_t> pixels,memory;
    std::vector<std::pair<uint32_t,dbg_instr_t>> instructions;
    std::vector<hwaddr> breakpoints;
    std::vector<watchpoint_t> watchpoints;
    dbg_image_info_t image{};
    dbg_mapping_t mappings[4]{};
    dbg_counter_t counters[8]{};
    uint32_t shown_pc=0xffffffff;
    Panel(dbg_ui_t* u,int i):Fl_Group(0,0,300,240),ui(u),id(i) {
        box(FL_FLAT_BOX);begin();
        if(i==2||i==3||i==4) {
            location=new Fl_Input(8,4,180,24);location->value("0000");location->when(FL_WHEN_ENTER_KEY_ALWAYS);
            location->tooltip("Hex address or symbol; Enter to navigate/add breakpoint");
            location->callback([](Fl_Widget*,void* p){ auto q=(Panel*)p;bool valid;auto addr=address(q->ui,q->location->value(),&valid);
                if(!valid){q->ui->message="Unknown address or symbol";return;}
                q->start=addr;if(q->id==2)debugger_set_breakpoint(q->ui->host.debugger,addr);
                q->ui->last_refresh=0;q->redraw();},this);
        }
        if(i==4||i==7) {
            choice=new Fl_Choice(194,4,98,24);
            if(i==4){choice->add("256 bytes|1 KiB|4 KiB|16 KiB|64 KiB");choice->value(0);}
            else {choice->add("Layer 0|Layer 1|Tileset|Palette|Font");choice->value(0);}
            choice->callback([](Fl_Widget*,void* p){auto q=(Panel*)p;const uint32_t ranges[]={256,1024,4096,16384,65536};
                if(q->id==4)q->range=ranges[q->choice->value()];q->scroll_x=q->scroll_y=0;q->ui->last_refresh=0;q->redraw();},this);
        }
        if(i==1) {
            const char* labels[]={"AF","BC","DE","HL","PC","SP","IX","IY","AF'","BC'","DE'","HL'","IR","Flags"};
            for(int j=0;j<14;j++) {
                auto in=new Fl_Input(40+(j%2)*130,8+(j/2)*28,84,24,labels[j]);registers[j]=in;
                if(j==13){in->readonly(1);continue;}
                in->maximum_size(4);in->when(FL_WHEN_ENTER_KEY_ALWAYS);
                in->callback([](Fl_Widget* w,void* p){auto q=(Panel*)p;if(!q->ui->snapshot.paused)return;
                    bool valid;auto v=address(q->ui,((Fl_Input*)w)->value(),&valid);if(!valid)return;
                    debugger_get_registers(q->ui->host.debugger,&q->regs);
                    uint16_t* vals[]={&q->regs.af,&q->regs.bc,&q->regs.de,&q->regs.hl,&q->regs.pc,&q->regs.sp,&q->regs.ix,&q->regs.iy,
                        &q->regs.af_,&q->regs.bc_,&q->regs.de_,&q->regs.hl_,&q->regs.ir};
                    for(int k=0;k<13;k++)if(q->registers[k]==w)*vals[k]=v;
                    debugger_set_registers(q->ui->host.debugger,&q->regs);q->ui->last_refresh=0;},this);
                in->tooltip("Edit hex value, Enter to apply while paused; double-click label navigates memory");
            }
        }
        canvas=new Canvas(this,0,i==1?210:(location||choice?34:0),300,i==1?30:206);
        resizable(canvas);end();hide();
    }
    void resize(int x,int y,int w,int h) override {
        Fl_Group::resize(x,y,w,h);
        int row=ui->theme.row_height,pad=ui->theme.spacing;
        int controls=location||choice?row+pad:0;
        if(location)location->resize(x+pad,y+pad,std::max(70,w-(choice?112:2*pad)),row);
        if(choice)choice->resize(x+(location?std::max(82,w-106):pad),y+pad,location?100:std::max(100,w-2*pad),row);
        if(id==1){for(int i=0;i<14;i++)registers[i]->resize(x+36+(i%2)*(w/2),y+pad+(i/2)*(row+pad),std::max(25,w/2-44),row);controls=7*(row+pad)+pad;}
        canvas->resize(x,y+controls,w,std::max(1,h-controls));
    }
    void refresh() {
        auto d=ui->host.debugger;
        if(id==0||id==7) {
            dbg_image_info_t info{};int view=id==0?-1:choice->value();
            auto result=debugger_image_copy(d,view,&info,nullptr,0);
            if(result==DBG_CAPACITY){pixels.resize(info.stride*info.height);if(debugger_image_copy(d,view,&info,pixels.data(),pixels.size())==DBG_OK)image=info;}
        } else if(id==1) {
            debugger_get_registers(d,&regs);
            const uint16_t values[]={regs.af,regs.bc,regs.de,regs.hl,regs.pc,regs.sp,regs.ix,regs.iy,regs.af_,regs.bc_,regs.de_,regs.hl_,regs.ir,regs.f};
            for(int i=0;i<14;i++){registers[i]->readonly(!ui->snapshot.paused||i==13);if(Fl::focus()!=registers[i])registers[i]->value(hex(values[i],i==13?2:4,ui->upper).c_str());}
        } else if(id==2) {
            hwaddr b[DBG_MAX_POINTS];int n=debugger_get_breakpoints(d,b,DBG_MAX_POINTS);breakpoints.assign(b,b+n);
            watchpoint_t w[DBG_MAX_POINTS];n=debugger_get_watchpoints(d,w,DBG_MAX_POINTS);watchpoints.assign(w,w+n);
        } else if(id==3) {
            if(shown_pc!=ui->snapshot.pc && ui->snapshot.paused){shown_pc=ui->snapshot.pc;start=shown_pc;if(Fl::focus()!=location)location->value(hex(start,4).c_str());}
            instructions.clear();unsigned addr=start;
            for(int j=0;j<128;j++){dbg_instr_t in{};int n=debugger_disassemble_address(d,addr,&in);if(n<=0)break;instructions.push_back({addr,in});addr=(addr+n)&65535;}
        } else if(id==4) {
            unsigned size=std::min(range,65536-start);memory.resize(size);debugger_memory_read(d,DBG_VIRTUAL,start,memory.data(),size);
        } else if(id==5)debugger_mappings(d,mappings);
        else if(id==6)debugger_counters(d,counters);
        canvas->redraw();
    }
};

void Canvas::draw() {
    auto p=panel;auto u=p->ui;int row=u->theme.row_height,pad=u->theme.spacing;
    fl_push_clip(x(),y(),w(),h());fl_color(u->color("surface"));fl_rectf(x(),y(),w(),h());
    int baseline=y()+row-6;
    if(p->id==0||p->id==7) {
        if(!p->pixels.empty() && p->image.width) {
            int iw=p->image.width,ih=p->image.height;
            int dw,dh,ox,oy;
            if(p->id==0){double fit=std::min(double(w())/iw,double(h()-row)/ih);dw=std::max(1,int(iw*fit));dh=std::max(1,int(ih*fit));ox=x()+(w()-dw)/2;oy=y()+(h()-row-dh)/2;}
            else {dw=iw*u->scale;dh=ih*u->scale;ox=x()-p->scroll_x;oy=y()-p->scroll_y;}
            // Explicit nearest-neighbor scaling, independent of FLTK image filters.
            int left=std::max(x(),ox),top=std::max(y(),oy),right=std::min(x()+w(),ox+dw),bottom=std::min(y()+h()-row,oy+dh);
            if(right>left && bottom>top){std::vector<unsigned char> scaled((right-left)*(bottom-top)*3);
                for(int yy=top;yy<bottom;yy++)for(int xx=left;xx<right;xx++) {
                    unsigned src=((yy-oy)*ih/dh)*p->image.stride+((xx-ox)*iw/dw)*4;
                    auto dst=&scaled[((yy-top)*(right-left)+xx-left)*3];std::memcpy(dst,&p->pixels[src],3);
                }
                fl_draw_image(scaled.data(),left,top,right-left,bottom-top,3);
            }
            std::string footer=p->id==0?(u->snapshot.paused?"Paused  ":"Running  ")+hex(u->snapshot.pc,4)+"  Click video for keyboard":"Wheel: scroll  Shift: horizontal  Pixel "+std::to_string(p->hover);
            text(u,u->snapshot.paused?"paused":"muted",footer,x()+pad,y()+h()-5,false);
        } else text(u,"muted","Waiting for frame",x()+pad,baseline,false);
    } else if(p->id==1) {
        const char* flags="SZ5H3PNC";std::string s;
        for(int i=0;i<8;i++){s+=p->regs.f&(128>>i)?flags[i]:'.';s+=' ';}
        text(u,"muted",s,x()+pad,baseline);
    } else if(p->id==2) {
        int line=0;
        for(auto addr:p->breakpoints){int yy=baseline+(line++-p->scroll_y)*row;if(yy<y())continue;
            const char* symbol=debugger_get_symbol(u->host.debugger,addr);
            text(u,"breakpoint","● "+hex(addr,4)+"  "+(symbol?symbol:"")+"   ×",x()+pad,yy);}
        if(p->breakpoints.empty())text(u,"muted","Enter address above to add",x()+pad,baseline,false);
        for(auto wp:p->watchpoints){int yy=baseline+(line++-p->scroll_y)*row;text(u,"warning","Watch "+hex(wp.addr,4)+"  "+(wp.type==WATCHPOINT_RW?"RW":wp.type==WATCHPOINT_READ?"R":"W"),x()+pad,yy);}
    } else if(p->id==3) {
        for(size_t i=p->scroll_y;i<p->instructions.size();i++) {
            auto& [addr,in]=p->instructions[i];int yy=baseline+(int(i)-p->scroll_y)*row;if(yy>y()+h())break;
            bool pc=addr==u->snapshot.pc,bp=debugger_is_breakpoint_set(u->host.debugger,addr);
            if(pc){fl_color(u->color("current"));fl_rectf(x(),yy-row+6,w(),row);}
            text(u,bp?"breakpoint":"muted",bp?"●":" ",x()+pad,yy);
            text(u,pc?"link":"text",hex(addr,4,u->upper)+"  "+in.instruction,x()+24,yy);
            if(in.label[0])text(u,"muted",in.label,x()+280,yy);
        }
    } else if(p->id==4) {
        fl_font(u->mono_font,u->theme.mono_size);int cell=int(fl_width("0"))+1;
        int columns=w()>cell*76?16:8;
        for(int i=p->scroll_y*columns;i<(int)p->memory.size();i+=columns) {
            int yy=baseline+(i/columns-p->scroll_y)*row;if(yy>y()+h())break;
            text(u,"muted",hex(p->start+i,4,u->upper),x()+pad,yy);
            for(int j=0;j<columns&&i+j<(int)p->memory.size();j++) {
                int hx=x()+pad+6*cell+j*3*cell,ax=x()+pad+(7+columns*3+j)*cell;
                if(p->hover==i+j){fl_color(u->color("selection"));fl_rectf(hx,yy-row+6,2*cell,row);fl_rectf(ax,yy-row+6,cell,row);}
                text(u,"text",hex(p->memory[i+j],2,u->upper),hx,yy);
                text(u,"muted",glyph(p->memory[i+j],u->cp437),ax,yy);
            }
        }
    } else if(p->id==5) {
        text(u,"muted","Virtual   Page   Physical / Device",x()+pad,baseline);
        for(int i=0;i<4;i++){auto& m=p->mappings[i];int yy=baseline+(i*2+1)*row;
            text(u,"link",hex(m.virtual_address,4)+"     "+hex(m.page,2)+"     "+hex(m.physical_address,6),x()+pad,yy);
            text(u,"muted",m.device,x()+pad,yy+row);}
    } else if(p->id==6) {
        text(u,"muted","ID  State     Last / Min / Max µs",x()+pad,baseline);
        for(int i=0;i<8;i++) { auto& c=p->counters[i];int yy=baseline+(i*2+1)*row;
            text(u,c.break_on_update?"breakpoint":"text",std::to_string(i)+(c.break_on_update?" * ":"   ")+(c.running?"running  ":"stopped  ")+std::to_string(c.last_us),x()+pad,yy);
            text(u,"muted",std::to_string(c.minimum_us)+" / "+std::to_string(c.maximum_us)+"  avg "+std::to_string(c.samples?c.total_us/c.samples:0)+"  n="+std::to_string(c.samples),x()+pad,yy+row);
        }
    }
    fl_pop_clip();
}
int Canvas::handle(int event) {
    auto p=panel;auto u=p->ui;int row=u->theme.row_height;
    if(event==FL_UNFOCUS||event==FL_HIDE){if(p->id==0){p->captured=false;u->release();}return 1;}
    if(event==FL_FOCUS)return 1;
    if(event==FL_PUSH) {
        take_focus();int index=(Fl::event_y()-y())/row+p->scroll_y;
        if(p->id==0){p->mouse_x=Fl::event_x();p->mouse_y=Fl::event_y();if(Fl::event_button()==FL_MIDDLE_MOUSE)p->captured=!p->captured;return 1;}
        if(p->id==2){if(index<(int)p->breakpoints.size())debugger_clear_breakpoint(u->host.debugger,p->breakpoints[index]);
            else if(index-(int)p->breakpoints.size()<(int)p->watchpoints.size())debugger_remove_watchpoint(u->host.debugger,p->watchpoints[index-p->breakpoints.size()].addr);}
        if(p->id==3&&index<(int)p->instructions.size()) {auto addr=p->instructions[index].first;if(Fl::event_button()==FL_RIGHT_MOUSE)u->navigate(addr);else debugger_toggle_breakpoint(u->host.debugger,addr);}
        if(p->id==5&&index>0&&index<=8)u->navigate(p->mappings[(index-1)/2].virtual_address);
        if(p->id==6&&index>0&&index<=16){int i=(index-1)/2;debugger_counter_break(u->host.debugger,i,!p->counters[i].break_on_update);}
        u->last_refresh=0;return 1;
    }
    if(event==FL_MOUSEWHEEL) {
        int delta=Fl::event_dy();
        if(p->id==0){u->scale=std::clamp(u->scale-delta,1,6);return 1;}
        int& offset=Fl::event_state(FL_SHIFT)?p->scroll_x:p->scroll_y;
        int maximum=p->id==7?std::max(0,int(Fl::event_state(FL_SHIFT)?p->image.width:p->image.height)*u->scale-(Fl::event_state(FL_SHIFT)?w():h())):
            p->id==4?int(p->memory.size()/8):p->id==3?int(p->instructions.size())-1:0;
        offset=std::clamp(offset+delta*(p->id==7?32:3),0,std::max(0,maximum));redraw();return 1;
    }
    if((event==FL_MOVE||event==FL_DRAG||event==FL_RELEASE)&&p->id==0) {
        double scale=std::max(.001,std::min(double(w())/std::max(1u,p->image.width),double(h()-row)/std::max(1u,p->image.height)));
        uint32_t buttons=(Fl::event_state(FL_BUTTON1)?1u:0u)|(Fl::event_state(FL_BUTTON3)?2u:0u);
        if(Fl::focus()==this)u->host.mouse(u->host.debugger,int((Fl::event_x()-p->mouse_x)/scale),int((Fl::event_y()-p->mouse_y)/scale),buttons);
        p->mouse_x=Fl::event_x();p->mouse_y=Fl::event_y();return 1;
    }
    if(event==FL_MOVE&&p->id==4){fl_font(u->mono_font,u->theme.mono_size);int cell=int(fl_width("0"))+1,cols=w()>cell*76?16:8;
        int pos=(Fl::event_x()-x()-u->theme.spacing)/cell,col=pos>=7+cols*3?pos-7-cols*3:(pos-6)/3;
        p->hover=col>=0&&col<cols?((Fl::event_y()-y())/row+p->scroll_y)*cols+col:-1;redraw();return 1;}
    if(event==FL_MOVE&&p->id==7){int px=(Fl::event_x()-x()+p->scroll_x)/u->scale,py=(Fl::event_y()-y()+p->scroll_y)/u->scale;
        p->hover=py*p->image.width+px;redraw();return 1;}
    if((event==FL_KEYDOWN||event==FL_KEYUP)&&p->id==0) {
        if(!u->passthrough&&Fl::event_state(FL_COMMAND))return 0;
        auto key=translate_key(Fl::event_key());if(key){u->host.key(u->host.debugger,key,event==FL_KEYDOWN);return 1;}
    }
    return Fl_Widget::handle(event);
}

struct DockSurface:Fl_Group {
    dbg_ui_t* ui;
    struct Leaf {DockNode* node;Rect rect;};
    struct Split {DockNode* node;Rect rect,full;};
    std::vector<Leaf> leaves;std::vector<Split> splits;
    DockNode* resizing=nullptr;Rect resize_rect{};
    DockSurface(dbg_ui_t* u,int x,int y,int w,int h):Fl_Group(x,y,w,h),ui(u){end();}
    void arrange(DockNode* n,Rect r) {
        if(!n)return;
        if(n->axis){int size=n->axis==1?r.w:r.h;int cut=std::clamp(int(size*n->ratio),std::min(100,size/2),std::max(size/2,size-100));
            Rect a=r,b=r,s=r;
            if(n->axis==1){a.w=std::max(1,cut-3);b.x+=cut+3;b.w=std::max(1,r.w-cut-3);s.x+=cut-3;s.w=6;}
            else{a.h=std::max(1,cut-3);b.y+=cut+3;b.h=std::max(1,r.h-cut-3);s.y+=cut-3;s.h=6;}
            splits.push_back({n,s,r});arrange(n->first.get(),a);arrange(n->second.get(),b);
        }else{
            leaves.push_back({n,r});int head=ui->theme.row_height;
            for(size_t i=0;i<n->tabs.size();i++){auto p=ui->panels[n->tabs[i]];if(p->parent()!=this)add(p);
                p->resize(r.x,r.y+head,r.w,std::max(1,r.h-head));if(int(i)==n->selected)p->show();else p->hide();}
        }
    }
    void update(){leaves.clear();splits.clear();arrange(ui->workspace.root.get(),{x(),y(),w(),h()});redraw();}
    void resize(int x,int y,int w,int h)override{Fl_Group::resize(x,y,w,h);update();}
    void draw()override{
        fl_push_clip(x(),y(),w(),h());fl_color(ui->color("background"));fl_rectf(x(),y(),w(),h());
        draw_children();int head=ui->theme.row_height;
        for(auto l:leaves){fl_color(ui->color("border"));fl_rect(l.rect.x,l.rect.y,l.rect.w,l.rect.h);
            int count=l.node->tabs.size(),width=std::max(32,(l.rect.w-24)/count);
            for(int i=0;i<count;i++){fl_color(ui->color(i==l.node->selected?"selection":"background"));fl_rectf(l.rect.x+i*width,l.rect.y,width,head);
                text(ui,"text",names[l.node->tabs[i]],l.rect.x+i*width+6,l.rect.y+head-6,false);}
            text(ui,"muted","×",l.rect.x+l.rect.w-18,l.rect.y+head-6,false);
        }
        if(ui->drag>=0){int mx=Fl::event_x_root()-window()->x(),my=Fl::event_y_root()-window()->y();
            for(auto l:leaves)if(l.rect.contains(mx,my)){Rect r=l.rect;int edge=target_edge(r,mx,my);
                if(edge==1)r.w/=3;else if(edge==2){r.x+=r.w*2/3;r.w/=3;}else if(edge==3)r.h/=3;else if(edge==4){r.y+=r.h*2/3;r.h/=3;}
                fl_color(ui->color("link"));fl_line_style(FL_SOLID,3);fl_rect(r.x+2,r.y+2,r.w-4,r.h-4);fl_line_style(0);}}
        fl_pop_clip();
    }
    static int target_edge(Rect r,int x,int y){if(x<r.x+r.w/4)return 1;if(x>r.x+r.w*3/4)return 2;if(y<r.y+r.h/4)return 3;if(y>r.y+r.h*3/4)return 4;return 0;}
    int handle(int event)override{
        int mx=Fl::event_x(),my=Fl::event_y();
        if(event==FL_PUSH){for(auto s:splits)if(s.rect.contains(mx,my)){resizing=s.node;resize_rect=s.full;return 1;}
            for(auto l:leaves)if(Rect{l.rect.x,l.rect.y,l.rect.w,ui->theme.row_height}.contains(mx,my)){
                int id=l.node->tabs[l.node->selected];
                if(mx>l.rect.x+l.rect.w-24){ui->hide_panel(id);return 1;}
                int width=std::max(32,(l.rect.w-24)/(int)l.node->tabs.size());int index=std::min(int(l.node->tabs.size())-1,(mx-l.rect.x)/width);
                l.node->selected=index;id=l.node->tabs[index];ui->drag=id;ui->drag_x=Fl::event_x_root();ui->drag_y=Fl::event_y_root();update();return 1;
            }}
        if(event==FL_DRAG){if(resizing){resizing->ratio=std::clamp(resizing->axis==1?double(mx-resize_rect.x)/resize_rect.w:double(my-resize_rect.y)/resize_rect.h,.1,.9);update();return 1;}
            if(ui->drag>=0){redraw();return 1;}}
        if(event==FL_RELEASE){if(resizing){resizing=nullptr;return 1;}if(ui->drag>=0){
            if(std::abs(Fl::event_x_root()-ui->drag_x)+std::abs(Fl::event_y_root()-ui->drag_y)>8)ui->drop(Fl::event_x_root(),Fl::event_y_root());
            ui->drag=-1;redraw();return 1;}}
        return Fl_Group::handle(event);
    }
};
struct FloatWindow:Fl_Double_Window {
    dbg_ui_t* ui;int id;
    FloatWindow(dbg_ui_t* u,int id,int x,int y,int w,int h):Fl_Double_Window(x,y,w,h,names[id]),ui(u),id(id){end();}
    void draw()override{Fl_Double_Window::draw();text(ui,"muted","Drag here to dock · double-click to return",8,ui->theme.row_height-6,false);}
    int handle(int event)override{
        if(event==FL_PUSH&&Fl::event_y()<ui->theme.row_height){if(Fl::event_clicks()){ui->workspace.dock(id,-1,0);ui->rebuild=true;return 1;}
            ui->drag=id;return 1;}
        if(event==FL_DRAG&&ui->drag==id){ui->surface->redraw();return 1;}
        if(event==FL_RELEASE&&ui->drag==id){ui->drop(Fl::event_x_root(),Fl::event_y_root());ui->drag=-1;return 1;}
        return Fl_Double_Window::handle(event);
    }
};
void dbg_ui_t::drop(int x,int y){release();int mx=x-window->x(),my=y-window->y();
    for(auto l:surface->leaves)if(l.rect.contains(mx,my)){workspace.dock(drag,l.node->tabs[l.node->selected],DockSurface::target_edge(l.rect,mx,my));rebuild=true;return;}
    workspace.detach(drag,x,y);rebuild=true;
}
void dbg_ui_t::hide_panel(int id){release();workspace.hide(id);rebuild=true;}
void dbg_ui_t::layout(){
    release();for(auto p:panels){p->hide();if(p->parent())p->parent()->remove(p);}
    for(auto [id,w]:floating){w->hide();delete w;}floating.clear();
    for(auto& f:workspace.floating){int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh,f.x,f.y);
        f.w=std::clamp(f.w,160,sw);f.h=std::clamp(f.h,100,sh);f.x=std::clamp(f.x,sx,sx+sw-f.w);f.y=std::clamp(f.y,sy,sy+sh-f.h);
        auto w=new FloatWindow(this,f.panel,f.x,f.y,f.w,f.h);w->size_range(200,160);w->begin();w->add(panels[f.panel]);w->end();
        panels[f.panel]->resize(0,theme.row_height,f.w,f.h-theme.row_height);w->resizable(panels[f.panel]);
        w->callback([](Fl_Widget* w,void* data){auto u=(dbg_ui_t*)data;for(auto [id,f]:u->floating)if(f==w){u->hide_panel(id);break;}},this);
        floating[f.panel]=w;panels[f.panel]->show();if(shown)w->show();}
    surface->update();apply_theme();rebuild=false;
}
static Fl_Font resolve_font(const std::string& name,Fl_Font fallback){int count=Fl::set_fonts(nullptr);for(int i=0;i<count;i++)if(name==Fl::get_font_name((Fl_Font)i))return (Fl_Font)i;return fallback;}
void dbg_ui_t::apply_theme(){
    ui_font=resolve_font(theme.ui_font,FL_HELVETICA);mono_font=resolve_font(theme.mono_font,FL_COURIER);
    auto bg=theme.colors.at("background"),fg=theme.colors.at("text"),surface_color=theme.colors.at("surface");
    Fl::background(bg>>16,(bg>>8)&255,bg&255);Fl::foreground(fg>>16,(fg>>8)&255,fg&255);Fl::background2(surface_color>>16,(surface_color>>8)&255,surface_color&255);
    std::function<void(Fl_Widget*)> style=[&](Fl_Widget* w){w->color(color("surface"));w->labelcolor(color("text"));w->selection_color(color("selection"));w->labelfont(ui_font);w->labelsize(theme.font_size);
        if(auto i=dynamic_cast<Fl_Input*>(w)){i->textcolor(color("text"));i->textfont(mono_font);i->textsize(theme.mono_size);i->cursor_color(color("link"));}
        if(auto m=dynamic_cast<Fl_Menu_*>(w)){m->textcolor(color("text"));m->textfont(ui_font);m->textsize(theme.font_size);}
        if(auto g=dynamic_cast<Fl_Group*>(w))for(int i=0;i<g->children();i++)style(g->child(i));};
    style(window);for(auto [id,w]:floating){style(w);panels[id]->resize(0,theme.row_height,w->w(),w->h()-theme.row_height);w->redraw();}
    for(auto p:panels)p->resize(p->x(),p->y(),p->w(),p->h());surface->update();window->redraw();
}
void dbg_ui_t::select_theme(const std::string& name){Theme next;std::string error;
    if(name=="Dark"||name=="Light")next=Theme::preset(name=="Light");
    else if(!Theme::load(directory+"/themes/"+name,next,error)){message=error;return;}
    theme=std::move(next);theme_name=name;apply_theme();
}
void dbg_ui_t::save(){
    for(auto& f:workspace.floating){auto w=floating.at(f.panel);f.x=w->x();f.y=w->y();f.w=w->w();f.h=w->h();}
    if(!workspace.save(directory+"/fltk-workspace.ini")){message="Cannot save workspace";return;}
    std::ofstream f(directory+"/fltk-preferences.ini");f<<"theme="<<theme_name<<"\nwidth="<<window->w()<<"\nheight="<<window->h()<<"\nupper="<<upper<<"\ncp437="<<cp437<<"\n";
    message=f?"Workspace and theme saved":"Cannot save preferences";
}
void dbg_ui_t::navigate(uint32_t addr){auto p=panels[4];p->start=addr;p->scroll_y=0;p->location->value(hex(addr,4).c_str());workspace.show(4);if(auto n=workspace.leaf(4))n->selected=std::find(n->tabs.begin(),n->tabs.end(),4)-n->tabs.begin();rebuild=true;last_refresh=0;}
static void menu_callback(Fl_Widget* w,void* data){auto u=(dbg_ui_t*)data;auto menu=(Fl_Menu_Bar*)w;char path[512];menu->item_pathname(path,sizeof(path));std::string s=path;
    if(s=="File/Debugger Off"){u->save();u->host.action(u->host.debugger,UI_OFF,0,0);}
    else if(s=="File/Save Config"){u->save();u->host.action(u->host.debugger,UI_SAVE,0,0);}
    else if(s=="File/Quit")u->command(DBG_STOP);
    else if(s=="CPU/Pause")u->command(DBG_PAUSE);else if(s=="CPU/Continue")u->command(DBG_CONTINUE);
    else if(s=="CPU/Step")u->command(DBG_STEP);else if(s=="CPU/Step Over")u->command(DBG_STEP_OVER);else if(s=="CPU/Reset")u->command(DBG_RESET);
    else if(s=="CPU/Toggle Breakpoint"){debugger_toggle_breakpoint(u->host.debugger,u->snapshot.pc);u->last_refresh=0;}
    else if(s=="View/Reset Layout"){u->workspace.reset();u->rebuild=true;}
    else if(s=="View/Keyboard Passthrough"){u->passthrough=!u->passthrough;u->release();u->host.action(u->host.debugger,UI_PASSTHROUGH,u->passthrough,0);}
    else if(s=="View/Uppercase Hex"){u->upper=!u->upper;u->last_refresh=0;}
    else if(s=="View/CP437"){u->cp437=!u->cp437;u->last_refresh=0;}
    else if(s.rfind("View/",0)==0){for(int i=0;i<8;i++)if(s==std::string("View/")+names[i]){u->workspace.show(i);if(auto n=u->workspace.leaf(i))n->selected=std::find(n->tabs.begin(),n->tabs.end(),i)-n->tabs.begin();u->rebuild=true;}}
    else if(s=="Theme/Reload")u->select_theme(u->theme_name);else if(s.rfind("Theme/",0)==0)u->select_theme(s.substr(6));
    else if(s=="Video/Scale Up")u->scale=std::min(6,u->scale+1);else if(s=="Video/Scale Down")u->scale=std::max(1,u->scale-1);
    else if(s=="Audio/Volume Up"||s=="Audio/Volume Down"){u->volume=std::clamp(u->volume+(s=="Audio/Volume Up"?10:-10),0,100);u->host.action(u->host.debugger,UI_VOLUME,u->volume,0);u->message="Volume "+std::to_string(u->volume)+"%";}
    else if(s=="SNES/Mouse/Reset Speed")u->host.action(u->host.debugger,UI_MOUSE_RESET,0,0);
    else if(s.rfind("SNES/",0)==0){int port=s.find("Port 1")!=s.npos?0:s.find("Port 2")!=s.npos?1:-1;
        if(s.find("Mouse")!=s.npos)u->host.action(u->host.debugger,UI_MOUSE_PORT,port,0);
        else {int index=s.find("Controller 1")!=s.npos?0:s.find("Controller 2")!=s.npos?1:s.find("Controller 3")!=s.npos?2:3;u->host.action(u->host.debugger,UI_CONTROLLER_PORT,index,port);}}
}
void dbg_ui_t::build_menu(){menu->clear();auto add=[&](const std::string& s,int key=0,int flags=0){menu->add(s.c_str(),key,menu_callback,this,flags);};
    add("File/Debugger Off",FL_COMMAND+FL_F+1);add("File/Save Config",FL_COMMAND+'s');add("File/Quit",FL_COMMAND+'q');
    add("CPU/Continue",FL_COMMAND+FL_F+5);add("CPU/Pause",FL_COMMAND+FL_F+6);add("CPU/Step Over",FL_COMMAND+FL_F+10);add("CPU/Step",FL_COMMAND+FL_F+11);
    add("CPU/Toggle Breakpoint",FL_COMMAND+FL_F+9);add("CPU/Reset",FL_COMMAND+FL_SHIFT+FL_BackSpace);
    for(auto name:names)add(std::string("View/")+name);add("View/Reset Layout");add("View/Keyboard Passthrough",0,FL_MENU_TOGGLE|(passthrough?FL_MENU_VALUE:0));
    add("View/Uppercase Hex",0,FL_MENU_TOGGLE|(upper?FL_MENU_VALUE:0));add("View/CP437",0,FL_MENU_TOGGLE|(cp437?FL_MENU_VALUE:0));
    add("Theme/Dark");add("Theme/Light");add("Theme/Reload");
    std::error_code ec;std::filesystem::create_directories(directory+"/themes",ec);
    for(auto& entry:std::filesystem::directory_iterator(directory+"/themes",ec))if(entry.path().extension()==".ini")add("Theme/"+entry.path().filename().string());
    add("Video/Scale Up",FL_COMMAND+FL_SHIFT+'=');add("Video/Scale Down",FL_COMMAND+FL_SHIFT+'-');
    add("Audio/Volume Up",FL_COMMAND+FL_SHIFT+'0');add("Audio/Volume Down",FL_COMMAND+FL_SHIFT+'9');
    for(std::string device:{"Mouse","Controller 1","Controller 2","Controller 3","Controller 4"})for(auto port:{"Detached","Port 1","Port 2"})add("SNES/"+device+"/"+port);
    add("SNES/Mouse/Reset Speed");
}
class MainWindow:public Fl_Double_Window {
    dbg_ui_t* ui;
public:MainWindow(dbg_ui_t* u,int w,int h):Fl_Double_Window(w,h,"Zeal 8-bit Debugger"),ui(u){}
    int handle(int event) override {
        if(event==FL_UNFOCUS)ui->release();
        return Fl_Double_Window::handle(event);
    }
};
extern "C" int debugger_ui_init(dbg_ui_t** out,const dbg_ui_init_args_t* args){
    if(!out||!args||!args->debugger||!args->config_directory)return -1;
    auto u=new dbg_ui_t;u->host=*args;u->directory=args->config_directory;u->volume=args->volume;u->passthrough=args->passthrough;
    Fl::scheme("gtk+");
    int width=args->width>600?args->width:1280,height=args->height>400?args->height:900;
    std::ifstream pref(u->directory+"/fltk-preferences.ini");std::string line;
    while(std::getline(pref,line)){auto eq=line.find('=');if(eq==line.npos)continue;auto key=line.substr(0,eq),value=line.substr(eq+1);
        if(key=="theme"&&value.find('/')==value.npos&&value.find('\\')==value.npos)u->theme_name=value;
        try{if(key=="width")width=std::clamp(std::stoi(value),640,4000);if(key=="height")height=std::clamp(std::stoi(value),480,3000);if(key=="upper")u->upper=std::stoi(value)!=0;if(key=="cp437")u->cp437=std::stoi(value)!=0;}catch(...){} }
    int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);width=std::min(width,sw);height=std::min(height,sh);
    u->window=new MainWindow(u,width,height);u->window->position(sx+(sw-width)/2,sy+(sh-height)/2);u->window->size_range(640,480);u->window->begin();
    u->menu=new Fl_Menu_Bar(0,0,width,28);u->build_menu();
    const char* labels[]={"Continue","Pause","Step","Step Over","Reset"};
    const dbg_command_t commands[]={DBG_CONTINUE,DBG_PAUSE,DBG_STEP,DBG_STEP_OVER,DBG_RESET};
    for(int i=0;i<5;i++){auto b=new Fl_Button(8+i*100,34,94,28,labels[i]);b->user_data(u);b->argument(commands[i]);
        b->callback([](Fl_Widget* w,void* data){auto u=(dbg_ui_t*)w->parent()->user_data();u->command((dbg_command_t)(intptr_t)data);});}
    u->window->user_data(u);
    u->surface=new DockSurface(u,6,70,width-12,height-104);u->status=new Fl_Box(6,height-28,width-12,24);u->status->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    u->window->resizable(u->surface);u->window->end();
    u->window->callback([](Fl_Widget*,void* data){auto u=(dbg_ui_t*)data;u->save();u->command(DBG_STOP);},u);
    Fl_Group::current(nullptr);for(int i=0;i<8;i++)u->panels[i]=new Panel(u,i);
    if(!u->workspace.load(u->directory+"/fltk-workspace.ini"))for(int i=0;i<8;i++)if(args->hidden_panels&(1u<<i))u->workspace.hide(i);
    u->layout();u->select_theme(u->theme_name);*out=u;return 0;
}
extern "C" void debugger_ui_deinit(dbg_ui_t* u){if(!u)return;u->save();u->release();
    for(auto p:u->panels){if(p->parent())p->parent()->remove(p);delete p;}
    for(auto [id,w]:u->floating)delete w;delete u->window;delete u;
}
extern "C" void debugger_ui_show(dbg_ui_t* u,bool visible){if(!u)return;u->shown=visible;u->release();if(visible)u->window->show();else u->window->hide();for(auto [id,w]:u->floating){if(visible)w->show();else w->hide();}}
#ifdef CONFIG_FLTK_TESTS
static void smoke_tick(dbg_ui_t* u) {
    const char* path=std::getenv("ZEAL_FLTK_SMOKE"); if(!path)return;
    static double started=now();static int phase=0;
    double elapsed=now()-started;
    if(phase==0&&elapsed>.2){u->command(DBG_CONTINUE);phase++;}
    else if(phase==1&&elapsed>1){u->command(DBG_PAUSE);u->workspace.detach(6,50,50);u->rebuild=true;phase++;}
    else if(phase==2&&elapsed>1.2){u->workspace.dock(6,5,0);u->rebuild=true;u->select_theme("Light");phase++;}
    else if(phase==3&&elapsed>1.4){u->workspace.reset();u->rebuild=true;u->select_theme("Dark");u->command(DBG_STEP);phase++;}
    else if(phase==4&&elapsed>1.6){
        u->window->make_current();Fl::flush();
        auto pixels=fl_read_image(nullptr,0,0,u->window->w(),u->window->h(),0);
        if(pixels){std::ofstream f(std::string(path)+".ppm",std::ios::binary);f<<"P6\n"<<u->window->w()<<" "<<u->window->h()<<"\n255\n";
            f.write((char*)pixels,u->window->w()*u->window->h()*3);delete[] pixels;}
        phase++;
    } else if(phase==5&&elapsed>2){u->save();std::fprintf(stdout,"FLTK_SMOKE_OK pc=%04x frame=%llu\n",u->snapshot.pc,(unsigned long long)u->panels[0]->image.generation);u->command(DBG_STOP);phase++;}
}
#endif
extern "C" void debugger_ui_poll(dbg_ui_t* u){if(!u||!u->shown)return;Fl::check();if(u->rebuild)u->layout();
    debugger_ui_refresh(u);
#ifdef CONFIG_FLTK_TESTS
    smoke_tick(u);
#endif
    if(u->snapshot.paused)Fl::wait(.005);
}
extern "C" void debugger_ui_refresh(dbg_ui_t* u){if(!u||!u->shown)return;double time=now();if(time-u->last_refresh<1.0/30)return;u->last_refresh=time;
    debugger_snapshot(u->host.debugger,&u->snapshot);for(auto p:u->panels)if(p->visible_r())p->refresh();
    std::string s=(u->snapshot.paused?"Paused":"Running")+std::string("  PC $")+hex(u->snapshot.pc,4)+"    Theme: "+u->theme_name+"    "+u->message;u->status->copy_label(s.c_str());
}
extern "C" bool debugger_ui_main_view_focused(const dbg_ui_t* u){return u&&u->shown&&u->panels[0]->canvas==Fl::focus();}
extern "C" dbg_vram_t debugger_ui_vram_panel_opened(const dbg_ui_t* u){return u&&u->shown&&u->panels[7]->visible_r()?(dbg_vram_t)u->panels[7]->choice->value():DBG_VIEW_NONE;}
extern "C" void debugger_ui_scale(dbg_ui_t* u,int delta){if(u)u->scale=std::clamp(u->scale+delta,1,6);}
