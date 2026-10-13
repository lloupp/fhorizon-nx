#include "deko_backend.hpp"
#include "pinyon_surfaces.hpp"
#include "scene.hpp"
#include <switch.h>
#include <cstdio>
#include "buffer_vsh_bin.h"
#include "pinyon_sample_bin.h"
using namespace fhnx::gpu;using namespace fhnx::scene;using namespace rex::graphics;
extern "C" {u32 __nx_heap_size=64*1024*1024;}
static FILE* trace_file;
static void trace(const char* text){char s[256];int n=snprintf(s,sizeof(s),"FHNX_GRAPHICS %s\n",text);svcOutputDebugString(s,n);if(trace_file){fwrite(s,1,n,trace_file);fflush(trace_file);}}
static void gate(bool ok,const char* name){require(ok,name);trace(name);}
struct Geometry {Buffer vertices,indices;uint32_t count;};
static Geometry upload(DekoBackend& gpu,const Mesh& m){return {gpu.buffer(BufferKind::vertex,uint32_t(m.vertices.size()*sizeof(Vertex)),m.vertices.data()),gpu.buffer(BufferKind::index16,uint32_t(m.indices.size()*2),m.indices.data()),uint32_t(m.indices.size())};}
int main(){trace_file=fopen("sdmc:/fhnx_scene_trace.log","w");trace("boot");try{
 DekoBackend gpu(32*1024*1024);PinyonSurfaces native(gpu,8*1024*1024);
 auto vs=gpu.shader(buffer_vsh_bin,buffer_vsh_bin_size),fs=gpu.shader(pinyon_sample_bin,pinyon_sample_bin_size);
 trace("scene_shaders_ready");
 Mesh ground,road,car,screen;
 ground.plane(-7,-7,7,7,-.04f,{.12f,.5f,.16f,1},14);
 const std::array<float,4> grey={.45f,.45f,.45f,1};
 road.plane(-5.5f,-5.5f,5.5f,-3.5f,0,grey,8);road.plane(-5.5f,3.5f,5.5f,5.5f,0,grey,8);road.plane(-5.5f,-3.5f,-3.5f,3.5f,0,grey,8);road.plane(3.5f,-3.5f,5.5f,3.5f,0,grey,8);
 car.box({0,.28f,0},{.55f,.22f,.9f},{.08f,.24f,1,1});car.box({0,.62f,-.1f},{.38f,.15f,.45f},{.15f,.7f,1,1});
 for(float x:{-.57f,.57f})for(float z:{-.57f,.57f})car.box({x,.18f,z},{.12f,.18f,.23f},{.06f,.06f,.06f,1});
 screen.quad({-1,-1,.1f},{1,-1,.1f},{1,1,.1f},{-1,1,.1f},{1,1,1,1});
 // Render-target rows use top-left origin; invert V for fullscreen sampling.
 for(auto& vertex:screen.vertices)vertex.uv[1]=1-vertex.uv[1];
 auto g=upload(gpu,ground),r=upload(gpu,road),c=upload(gpu,car),s=upload(gpu,screen);
 trace("scene_buffers_ready");
 auto scene_constants=gpu.buffer(BufferKind::uniform,sizeof(Constants),nullptr),car_constants=gpu.buffer(BufferKind::uniform,sizeof(Constants),nullptr);Constants screen_u{identity()};auto screen_constants=gpu.buffer(BufferKind::uniform,sizeof(Constants),&screen_u);
 auto texture=gpu.create({16,16});uint32_t checker[256];for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)checker[y*16+x]=((x/4+y/4)&1)?0xffbfbfbf:0xffffffff;gpu.uploadRGBA8(texture,checker,sizeof(checker));
 auto color=Fh1SurfaceKey::Color(0,Fh1PitchTiles(480,2),2,xenos::ColorRenderTargetFormat::k_8_8_8_8,false),depth=Fh1SurfaceKey::Depth(512,Fh1PitchTiles(480,2),2,xenos::DepthRenderTargetFormat::kD24S8),resolved=Fh1SurfaceKey::Color(1536,Fh1PitchTiles(480,0),0,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);
 native.acquire(color,480,270);native.acquire(depth,480,270);native.acquire(resolved,480,270);
 trace("scene_surfaces_ready");
 auto draw=[&](Geometry geometry,Buffer constants,Stencil stencil){gpu.vertex(geometry.vertices,layout());gpu.uniform(constants);Draw d{vs,fs};d.count=geometry.count;d.indices=geometry.indices;d.depth_test=d.depth_write=true;d.stencil=stencil;native.draw(d);};
 auto render=[&](float angle,bool close){float distance=close?13:17;Constants u{multiply(perspective(480.0f/270,.1f,60),lookAt({std::sin(angle)*distance,close?8.0f:11.0f,std::cos(angle)*distance},{0,0,0}))};Constants vehicle=u;vehicle.transform=multiply(u.transform,translate(0,0,4.5f));gpu.update(scene_constants,0,sizeof(u),&u);gpu.update(car_constants,0,sizeof(vehicle),&vehicle);
  gpu.begin();native.bind(color,&depth);native.clear(.04f,.08f,.14f,1,true);gpu.sample(texture,{Filter::nearest,Wrap::repeat});draw(g,scene_constants,Stencil::disabled);draw(r,scene_constants,Stencil::replace);
#ifndef FHNX_DEMO_SKIP_CAR
  draw(c,car_constants,Stencil::equal);
#endif
  native.resolve(color,resolved);native.transition(native.get(resolved),Usage::readback);gpu.submit();};
 float angle=.65f;bool close=false;render(angle,close);auto pixels=gpu.readback(native.get(resolved).image);unsigned blue=0,grass=0;
 for(auto p:pixels){unsigned red=p&255,green=(p>>8)&255,b=(p>>16)&255;if(b>120 && b>2*red && b>2*green)++blue;if(green>40 && green>2*red && green>2*b)++grass;}
 char line[256];snprintf(line,sizeof(line),"scene_readback blue=%u grass=%u corner=%08lx center=%08lx",blue,grass,(unsigned long)pixels[0],(unsigned long)pixels[135*480+240]);trace(line);
#ifndef FHNX_DEMO_SKIP_CAR
 bool correct_pixels=blue>20 && grass>1000;
#else
 bool correct_pixels=blue==0 && grass>1000;
#endif
 Image backs[]={gpu.create({960,540,1,Format::rgba8,true}),gpu.create({960,540,1,Format::rgba8,true})};Usage state[]={Usage::undefined,Usage::undefined};auto swap=gpu.swapchain(backs[0],backs[1]);
 PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);unsigned frames=0;bool analog_seen=false;u64 total_ns=0,max_ns=0;trace("resources_ready");
 while(appletMainLoop()){u64 start=armGetSystemTick();padUpdate(&pad);auto down=padGetButtonsDown(&pad);if(down&HidNpadButton_A){close=!close;trace("a_pressed");}if(down&HidNpadButton_Plus){trace("plus_exit");break;}
  auto stick=padGetStickPos(&pad,0);if(std::abs(stick.x)>5000 || std::abs(stick.y)>5000){angle+=float(stick.x)/32768*.025f;if(!analog_seen){analog_seen=true;trace("analog_moved");}}
  render(angle,close);int slot=gpu.acquire(swap);gpu.begin();gpu.transition(backs[slot],state[slot],Usage::target);gpu.target(backs[slot]);gpu.clear(0,0,0,1);gpu.vertex(s.vertices,layout());gpu.uniform(screen_constants);native.sample(resolved);Draw present{vs,fs};present.count=s.count;present.indices=s.indices;gpu.draw(present);gpu.transition(backs[slot],Usage::target,Usage::present);gpu.submit();gpu.present(swap,slot,backs[slot]);state[slot]=Usage::present;
  u64 elapsed=armTicksToNs(armGetSystemTick()-start);total_ns+=elapsed;if(elapsed>max_ns)max_ns=elapsed;if(++frames==2)trace("frames_submitted");
 }
 u64 ram=0;svcGetInfo(&ram,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);snprintf(line,sizeof(line),"telemetry frames=%u mean_frame_us=%llu max_frame_us=%llu buffers=%llu images=%llu process_used=%llu",frames,(unsigned long long)(frames?total_ns/frames/1000:0),(unsigned long long)(max_ns/1000),(unsigned long long)gpu.bufferBytes(),(unsigned long long)gpu.imageBytes(),(unsigned long long)ram);trace(line);
 gpu.destroySwapchain(swap);for(auto h:backs)gpu.destroy(h);native.shutdown();gpu.destroy(texture);for(auto mesh:{g,r,c,s}){gpu.destroy(mesh.vertices);gpu.destroy(mesh.indices);}gpu.destroy(scene_constants);gpu.destroy(car_constants);gpu.destroy(screen_constants);gpu.destroy(vs);gpu.destroy(fs);gate(gpu.imageBytes()==0 && gpu.bufferBytes()==0,"scene_resources_reclaimed");
#ifndef FHNX_DEMO_SKIP_CAR
 gate(correct_pixels,"scene_geometry_gpu_pixels_pass");
#else
 gate(correct_pixels,"negative_omitted_vehicle_pass");
#endif
 }catch(const std::exception& e){trace(e.what());trace("demo_failed");return 2;}trace("shutdown");return 0;}
