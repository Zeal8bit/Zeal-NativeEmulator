/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zeal.h"
#include "utils/paths.h"
#include "utils/notif.h"

static void host_action(dbg_t* dbg,dbg_host_action_t op,int32_t a,int32_t b)
{
    zeal_t* m=dbg->arg;
    switch(op) {
    case UI_OFF: zeal_debug_disable(m); break;
    case UI_SAVE: config_save(); break;
    case UI_VOLUME: config.audio.volume=a<0?0:a>100?100:a; SetMasterVolume(config.audio.volume/100.0f); break;
    case UI_PASSTHROUGH: config.debugger.keyboard_passthru=a!=0; break;
    case UI_MOUSE_PORT: snes_adapter_set_mouse_port(&m->snes_adapter,a); break;
    case UI_CONTROLLER_PORT: snes_adapter_set_controller_port(&m->snes_adapter,a,b); break;
    case UI_MOUSE_RESET: snes_adapter_reset_mouse_scale(&m->snes_adapter); break;
    }
}
static void host_key(dbg_t* dbg,uint32_t key,uint32_t down)
{
    zeal_t* m=dbg->arg;
    if(key>=384 || key==0) return;
    if(down) {
        if(!debugger_is_paused(dbg)) { key_pressed(&m->keyboard,key); m->frontend_keys[key]=true; }
    } else if(m->frontend_keys[key]) {
        key_released(&m->keyboard,key); m->frontend_keys[key]=false;
    }
}
static void host_release(dbg_t* dbg)
{
    zeal_t* m=dbg->arg;
    for(unsigned k=0;k<384;++k) if(m->frontend_keys[k]) host_key(dbg,k,0);
    m->frontend_mouse_dx=m->frontend_mouse_dy=0; m->frontend_mouse_buttons=0;
}
static void host_mouse(dbg_t* dbg,int32_t dx,int32_t dy,uint32_t buttons)
{
    zeal_t* m=dbg->arg;
    m->frontend_mouse_dx+=dx; m->frontend_mouse_dy+=dy; m->frontend_mouse_buttons=buttons;
}
void debugger_host_frontend_args(dbg_t* dbg,dbg_ui_init_args_t* args)
{
    *args=(dbg_ui_init_args_t){.debugger=dbg,.config_directory=get_config_dir(),
        .width=config.debugger.width,.height=config.debugger.height,.volume=config.audio.volume,
        .passthrough=config.debugger.keyboard_passthru,.action=host_action,.key=host_key,
        .release_input=host_release,.mouse=host_mouse};
    const char* keys[]={"DISPLAY","CPU","BKPOINT","DISASSEMBLER","MEMORY","MMU","SEMIHOST","VRAM"};
    for(unsigned i=0;i<8;++i) {
        char key[64]; snprintf(key,sizeof(key),"%s_HIDDEN",keys[i]);
        if(config_get(key,i==7)) args->hidden_panels|=1u<<i;
    }
}
