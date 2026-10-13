// Synthetic executor client; every graphics operation goes through Commands.
#include "deko_backend.hpp"
#include <switch.h>
#include <cstdio>
#include "adapter_vsh_bin.h"
#include "adapter_solid_bin.h"
#include "adapter_sample_bin.h"
using namespace fhnx::gpu;
static void trace(const char* event) {
    char line[256]; int n=std::snprintf(line,sizeof(line),"FHNX_GRAPHICS %s\n",event); svcOutputDebugString(line,n);
}
static void gate(bool ok,const char* name) { trace(name); require(ok,name); }

int main() {
    trace("boot");
    try {
        DekoBackend backend(32*1024*1024); Commands& gpu=backend;
        auto vertex=gpu.shader(adapter_vsh_bin,adapter_vsh_bin_size);
        auto solid=gpu.shader(adapter_solid_bin,adapter_solid_bin_size);
        auto sampled=gpu.shader(adapter_sample_bin,adapter_sample_bin_size);
        // Dynamic resource lifetime including stale-handle rejection.
        auto temporary=gpu.create({32,32}); gpu.destroy(temporary);
        bool rejected=false; try { gpu.destroy(temporary); } catch(...) {rejected=true;}
        gate(rejected,"stale_handle_rejected");
        // Independent near/far depth gate through the same resource API.
        auto depth_color=gpu.create({320,180});
        auto depth_z=gpu.create({320,180,1,Format::z24s8});
        gpu.begin(); gpu.transition(depth_color,Usage::undefined,Usage::target);
        gpu.transition(depth_z,Usage::undefined,Usage::target); gpu.target(depth_color,depth_z);
        gpu.clear(0,0,0,1,true);
        Draw near{vertex,solid}; near.depth_test=near.depth_write=true; gpu.draw(near);
        Draw far=near; far.first=3; gpu.draw(far);
        gpu.transition(depth_color,Usage::target,Usage::readback); gpu.submit();
        auto depth_pixels=backend.readback(depth_color);
        gate((depth_pixels[90*320+160]&0xffffff)==0xffffff,"readback_depth_pass");
        gpu.destroy(depth_color); gpu.destroy(depth_z);
        auto destination=gpu.create({320,180});
        auto msaa=gpu.create({320,180,4});
        auto depth=gpu.create({320,180,4,Format::z24s8});
        gpu.begin();
        gpu.transition(destination,Usage::undefined,Usage::target); gpu.target(destination); gpu.clear(0,0.7f,0,1);
        gpu.transition(msaa,Usage::undefined,Usage::target); gpu.transition(depth,Usage::undefined,Usage::target);
        gpu.target(msaa,depth); gpu.clear(1,0.5f,0,1,true);
        Draw mask{vertex,solid}; mask.color_write=false; mask.stencil=Stencil::replace; gpu.draw(mask);
        Draw fill{vertex,solid}; fill.first=3; fill.stencil=Stencil::equal;
#ifdef FHNX_ADAPTER_NO_STENCIL
        fill.stencil=Stencil::disabled;
#endif
        gpu.draw(fill);
        gpu.transition(msaa,Usage::target,Usage::resolve_source);
        gpu.transition(destination,Usage::target,Usage::resolve_dest);
#ifndef FHNX_ADAPTER_NO_RESOLVE
        gpu.resolve(msaa,destination);
#endif
        gpu.transition(destination,Usage::resolve_dest,Usage::readback); gpu.submit();
        auto pixels=backend.readback(destination);
        auto at=[&](unsigned x,unsigned y){return pixels[y*320+x]&0xffffff;};
        char evidence[256]; std::snprintf(evidence,sizeof(evidence),"readback corner=%06lx center=%06lx top=%06lx bytes=%llu",(unsigned long)at(8,8),(unsigned long)at(160,90),(unsigned long)at(160,16),(unsigned long long)backend.imageBytes()); trace(evidence);
#ifdef FHNX_ADAPTER_NO_RESOLVE
        gate((at(160,90)&0xff)==0 && ((at(160,90)>>8)&0xff)>140,"readback_negative_green_pass");
#else
        gate((at(8,8)&0xff)>240 && ((at(8,8)>>8)&0xff)>100 && ((at(8,8)>>16)&0xff)<10,"readback_clear_pass");
        gate(at(160,90)==0xff4c33,"readback_draw_pass");
#ifdef FHNX_ADAPTER_NO_STENCIL
        gate(at(160,16)==0xff4c33,"readback_negative_stencil_pass");
#else
        gate(at(160,16)!=0xff4c33,"readback_stencil_pass");
#endif
#endif
        gpu.begin(); gpu.transition(destination,Usage::readback,Usage::sampled); gpu.submit();
        // Stencil/depth source resources are no longer in flight; reclaim now.
        gpu.destroy(depth); gpu.destroy(msaa);
        auto back0=gpu.create({960,540,1,Format::rgba8,true});
        auto back1=gpu.create({960,540,1,Format::rgba8,true});
        Image backs[]={back0,back1}; Usage states[]={Usage::undefined,Usage::undefined};
        auto swap=backend.swapchain(back0,back1);
        trace("resources_ready");
        PadState pad; padConfigureInput(1,HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
        unsigned palette=0,frames=0; uint64_t start=armGetSystemTick();
        while(appletMainLoop()) {
            padUpdate(&pad); auto down=padGetButtonsDown(&pad);
            if(down&HidNpadButton_A) {palette^=1; trace("a_pressed");}
            if(down&HidNpadButton_Plus) {trace("plus_exit"); break;}
            int slot=backend.acquire(swap); require(slot>=0 && slot<2,"acquire slot");
            gpu.begin(); gpu.transition(backs[slot],states[slot],Usage::target); gpu.target(backs[slot]);
            gpu.clear(palette?0.45f:0.025f,palette?0.08f:0.06f,0.12f,1);
            gpu.sample(destination); gpu.draw({vertex,sampled});
            gpu.transition(backs[slot],Usage::target,Usage::present); gpu.submit();
            backend.present(swap,slot,backs[slot]); states[slot]=Usage::present;
            if(++frames==2) trace("frames_submitted");
        }
        std::snprintf(evidence,sizeof(evidence),"telemetry frames=%u elapsed_ms=%llu image_bytes=%llu",frames,(unsigned long long)armTicksToNs(armGetSystemTick()-start)/1000000,(unsigned long long)backend.imageBytes()); trace(evidence);
        backend.destroySwapchain(swap); gpu.destroy(back0); gpu.destroy(back1); gpu.destroy(destination);
        gate(backend.imageBytes()==0,"image_budget_reclaimed");
    } catch(const std::exception& e) {trace(e.what()); trace("adapter_failed"); return 2;}
    trace("shutdown"); return 0;
}
