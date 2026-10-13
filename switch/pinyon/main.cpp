#include "deko_backend.hpp"
#include "pinyon_surfaces.hpp"
#include <switch.h>
#include <cstdio>
#include "pinyon_vsh_bin.h"
#include "pinyon_solid_bin.h"
#include "pinyon_sample_bin.h"
using namespace fhnx::gpu;
using namespace rex::graphics;
extern "C" {u32 __nx_heap_size=64*1024*1024;}
static void trace(const char* text){char s[256];int n=snprintf(s,sizeof(s),"FHNX_GRAPHICS %s\n",text);svcOutputDebugString(s,n);}
static void gate(bool b,const char* label){require(b,label);trace(label);}
template<class F>static bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
 trace("boot");
 try{
  DekoBackend gpu(32*1024*1024);PinyonSurfaces native(gpu,8*1024*1024);
  auto v=gpu.shader(pinyon_vsh_bin,pinyon_vsh_bin_size),solid=gpu.shader(pinyon_solid_bin,pinyon_solid_bin_size),sample=gpu.shader(pinyon_sample_bin,pinyon_sample_bin_size);
  using K=Fh1SurfaceKey;
  auto color=K::Color(0,Fh1PitchTiles(320,2),2,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);
  auto depth=K::Depth(512,Fh1PitchTiles(320,2),2,xenos::DepthRenderTargetFormat::kD24S8);
  auto resolved=K::Color(1024,Fh1PitchTiles(320,0),0,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);
  auto copied=K::Color(1536,Fh1PitchTiles(320,0),0,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);
  auto& first=native.acquire(color,320,180);auto saved=first.image;
  gate(native.acquire(color,320,180).image.slot==saved.slot && native.creations==1 && native.cache_hits==1,"upstream_surface_identity_reuse_pass");
  native.acquire(depth,320,180);native.acquire(resolved,320,180);native.acquire(copied,320,180);
  auto bad=color;bad.format=2;
  gate(rejects([&]{native.acquire(bad,320,180);}),"unsupported_format_rejected");
  bad=color;bad.base_tiles=4;
  gate(rejects([&]{native.acquire(bad,320,180);}),"overlap_transfer_rejected");
  PinyonSurfaces tiny(gpu,1);
  gate(rejects([&]{tiny.acquire(resolved,320,180);}) && tiny.bytes()==0 && tiny.count()==0,"cache_budget_rollback_pass");
  gpu.begin();native.bind(copied);native.clear(0,0.7f,0,1);
  native.bind(color,&depth);native.clear(1,0.5f,0,1,true);
  Draw mask{v,solid};mask.color_write=false;mask.stencil=Stencil::replace;native.draw(mask);
  Draw fill{v,solid};fill.first=3;fill.stencil=Stencil::equal;native.draw(fill);
  native.resolve(color,resolved);
#ifndef FHNX_PINYON_SKIP_COPY
  native.copy(resolved,copied);
#endif
  native.transition(native.get(copied),Usage::readback);gpu.submit();
  auto pixels=gpu.readback(native.get(copied).image);
#ifdef FHNX_PINYON_SKIP_COPY
  gate((pixels[90*320+160]&0xffffff)==0x00b200,"negative_omitted_copy_green_pass");
#else
  gate((pixels[8*320+8]&0xffffff)==0x0080ff && (pixels[90*320+160]&0xffffff)==0xff4c33 && (pixels[16*320+160]&0xffffff)==0x0080ff,"native_resolve_copy_gpu_pixels_pass");
#endif
  Image backs[]={gpu.create({960,540,1,Format::rgba8,true}),gpu.create({960,540,1,Format::rgba8,true})};Usage state[]={Usage::undefined,Usage::undefined};
  auto swap=gpu.swapchain(backs[0],backs[1]);trace("resources_ready");
  PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);unsigned frames=0,palette=0;
  while(appletMainLoop()){
   padUpdate(&pad);auto down=padGetButtonsDown(&pad);
   if(down&HidNpadButton_A){palette^=1;trace("a_pressed");}
   if(down&HidNpadButton_Plus){trace("plus_exit");break;}
   int slot=gpu.acquire(swap);gpu.begin();gpu.transition(backs[slot],state[slot],Usage::target);gpu.target(backs[slot]);gpu.clear(palette?0.45f:0.025f,palette?0.08f:0.06f,0.12f,1);
   native.sample(copied);gpu.draw({v,sample});gpu.transition(backs[slot],Usage::target,Usage::present);gpu.submit();gpu.present(swap,slot,backs[slot]);state[slot]=Usage::present;
   if(++frames==2)trace("frames_submitted");
  }
  u64 ram=0;svcGetInfo(&ram,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);char line[256];snprintf(line,sizeof(line),"telemetry cache_bytes=%llu images=%llu process_used=%llu creations=%u hits=%u frames=%u",(unsigned long long)native.bytes(),(unsigned long long)gpu.imageBytes(),(unsigned long long)ram,native.creations,native.cache_hits,frames);trace(line);
  gpu.destroySwapchain(swap);gpu.destroy(backs[0]);gpu.destroy(backs[1]);native.shutdown();gate(gpu.imageBytes()==0,"native_resources_reclaimed");
 }catch(const std::exception& e){trace(e.what());return 2;}
 trace("shutdown");return 0;
}
