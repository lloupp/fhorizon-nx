#include "deko_backend.hpp"
#include "pinyon_surfaces.hpp"
#include <switch.h>
#include <cstdio>
#include "buffer_vsh_bin.h"
#include "pinyon_sample_bin.h"
using namespace fhnx::gpu;using namespace rex::graphics;
extern "C" {u32 __nx_heap_size=64*1024*1024;}
struct Vertex {float position[3],color[4],uv[2];};
struct alignas(16) Constants {float matrix[16];float tint[4];};
static Constants constants(float x,float r=1){Constants u{};u.matrix[0]=u.matrix[5]=u.matrix[10]=u.matrix[15]=1;u.matrix[12]=x;u.tint[0]=r;u.tint[1]=u.tint[2]=u.tint[3]=1;return u;}
static void trace(const char* text){char s[256];int n=snprintf(s,sizeof(s),"FHNX_GRAPHICS %s\n",text);svcOutputDebugString(s,n);}
static void gate(bool b,const char* label){require(b,label);trace(label);}
template<class F>static bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){trace("boot");try{
 DekoBackend gpu(16*1024*1024);PinyonSurfaces surfaces(gpu,4*1024*1024);
 auto v=gpu.shader(buffer_vsh_bin,buffer_vsh_bin_size),f=gpu.shader(pinyon_sample_bin,pinyon_sample_bin_size);
 uint32_t spirv[]={0x07230203,0,0,0};gate(rejects([&]{gpu.shader(spirv,sizeof(spirv));}),"spirv_is_not_dksh_rejected");
 auto disposable=gpu.shader(buffer_vsh_bin,buffer_vsh_bin_size);gpu.destroy(disposable);gate(rejects([&]{gpu.destroy(disposable);}),"stale_shader_rejected");
 Vertex vertices[]={{{-.35f,-.5f,.2f},{1,1,1,1},{0,0}},{{.35f,-.5f,.2f},{1,1,1,1},{1,0}},{{.35f,.5f,.2f},{1,1,1,1},{1,1}},{{-.35f,.5f,.2f},{1,1,1,1},{0,1}}};
 uint16_t indices[]={0,1,2,0,2,3};auto vb=gpu.buffer(BufferKind::vertex,sizeof(vertices),vertices),ib=gpu.buffer(BufferKind::index16,sizeof(indices),indices);
 uint16_t invalid_indices[]={0,1,99};auto bad_ib=gpu.buffer(BufferKind::index16,sizeof(invalid_indices),invalid_indices);
 auto left=constants(-.5f),right=constants(.5f);auto ul=gpu.buffer(BufferKind::uniform,sizeof(left),&left),ur=gpu.buffer(BufferKind::uniform,sizeof(right),&right);
 gate(rejects([&]{gpu.update(vb,sizeof(vertices),1,vertices);}),"buffer_update_bounds_rejected");
 auto temp=gpu.buffer(BufferKind::vertex,16,nullptr);gpu.destroy(temp);auto replacement=gpu.buffer(BufferKind::vertex,16,nullptr);
 gate(replacement.slot==temp.slot && replacement.generation!=temp.generation && rejects([&]{gpu.update(temp,0,4,vertices);}),"buffer_generation_reuse_pass");gpu.destroy(replacement);
 auto red=gpu.create({8,8}),green=gpu.create({8,8});uint32_t texture[64];for(auto& p:texture)p=0xff0000ff;gpu.uploadRGBA8(red,texture,sizeof(texture));for(auto& p:texture)p=0xff00ff00;gpu.uploadRGBA8(green,texture,sizeof(texture));
 auto key=Fh1SurfaceKey::Color(0,Fh1PitchTiles(320,0),0,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);auto depth=Fh1SurfaceKey::Depth(512,Fh1PitchTiles(320,0),0,xenos::DepthRenderTargetFormat::kD24S8);surfaces.acquire(key,320,180);surfaces.acquire(depth,320,180);
 VertexLayout layout{sizeof(Vertex),3,{{{0,3},{12,4},{28,2}}}};
 Draw indexed{v,f};indexed.count=6;indexed.depth_test=indexed.depth_write=true;indexed.indices=ib;
 auto render=[&]{gpu.begin();surfaces.bind(key,&depth);surfaces.clear(.025f,.06f,.12f,1,true);gpu.vertex(vb,layout);
   gate(rejects([&]{gpu.update(vb,0,4,vertices);}) && rejects([&]{gpu.destroy(vb);}),"inflight_cpu_update_destroy_rejected");
   Draw bad=indexed;bad.indices=bad_ib;bad.count=3;
   gate(rejects([&]{gpu.draw(bad);}),"out_of_range_index_rejected");
   gpu.uniform(ul);gpu.sample(red,{Filter::nearest,Wrap::clamp});surfaces.draw(indexed);
   gpu.uniform(ur);gpu.sample(green,{Filter::linear,Wrap::repeat});surfaces.draw(indexed);
   surfaces.transition(surfaces.get(key),Usage::readback);gpu.submit();};
 render();auto pixels=gpu.readback(surfaces.get(key).image);
 gate((pixels[90*320+80]&0xffffff)==0x0000ff && (pixels[90*320+240]&0xffffff)==0x00ff00,"indexed_multi_descriptor_pixels_pass");
 left=constants(-.5f,.5f);gpu.update(ul,0,sizeof(left),&left);render();pixels=gpu.readback(surfaces.get(key).image);
 char pixel_line[128];snprintf(pixel_line,sizeof(pixel_line),"updated_pixels left=%08lx right=%08lx",(unsigned long)pixels[90*320+80],(unsigned long)pixels[90*320+240]);trace(pixel_line);
 gate(((pixels[90*320+80]&0xffffff)==0x000080 || (pixels[90*320+80]&0xffffff)==0x00007f) && (pixels[90*320+240]&0xffffff)==0x00ff00,"uniform_update_isolated_pixels_pass");
 Image backs[]={gpu.create({960,540,1,Format::rgba8,true}),gpu.create({960,540,1,Format::rgba8,true})};Usage states[]={Usage::undefined,Usage::undefined};auto swap=gpu.swapchain(backs[0],backs[1]);
 PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);trace("resources_ready");unsigned frames=0,palette=0;
 while(appletMainLoop()){padUpdate(&pad);auto down=padGetButtonsDown(&pad);if(down&HidNpadButton_A){palette^=1;trace("a_pressed");}if(down&HidNpadButton_Plus){trace("plus_exit");break;}
  left=constants(-.5f,palette?1:.5f);gpu.update(ul,0,sizeof(left),&left);
  int slot=gpu.acquire(swap);gpu.begin();gpu.transition(backs[slot],states[slot],Usage::target);gpu.target(backs[slot]);gpu.clear(.025f,.06f,.12f,1);gpu.vertex(vb,layout);
  gpu.uniform(ul);gpu.sample(red,{Filter::nearest,Wrap::clamp});Draw flat=indexed;flat.depth_test=flat.depth_write=false;gpu.draw(flat);
  gpu.uniform(ur);gpu.sample(green,{Filter::linear,Wrap::repeat});gpu.draw(flat);gpu.transition(backs[slot],Usage::target,Usage::present);gpu.submit();gpu.present(swap,slot,backs[slot]);states[slot]=Usage::present;
  if(++frames==2)trace("frames_submitted");
 }
 u64 ram=0;svcGetInfo(&ram,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);char line[256];snprintf(line,sizeof(line),"telemetry buffers=%llu images=%llu process_used=%llu frames=%u",(unsigned long long)gpu.bufferBytes(),(unsigned long long)gpu.imageBytes(),(unsigned long long)ram,frames);trace(line);
 gpu.destroySwapchain(swap);for(auto image:backs)gpu.destroy(image);surfaces.shutdown();gpu.destroy(red);gpu.destroy(green);gpu.destroy(vb);gpu.destroy(ib);gpu.destroy(bad_ib);gpu.destroy(ul);gpu.destroy(ur);gpu.destroy(v);gpu.destroy(f);
 gate(gpu.bufferBytes()==0 && gpu.imageBytes()==0,"buffer_shader_resources_reclaimed");
 }catch(const std::exception& e){trace(e.what());trace("buffer_probe_failed");return 2;}trace("shutdown");return 0;}
