#include "deko_backend.hpp"
#include "pm4_executor.hpp"
#include "pm4_fixture.hpp"
#include "xenos_copy_fixture.hpp"
#include <switch.h>
#include <cstdio>
#include <cstring>
#include "buffer_vsh_bin.h"
#include "pinyon_sample_bin.h"
using namespace fhnx::gpu;using namespace rex::graphics;
extern "C" {u32 __nx_heap_size=64*1024*1024;}
static FILE* trace_file;
static void trace(const char* text){char s[320];int n=snprintf(s,sizeof(s),"FHNX_GRAPHICS %s\n",text);svcOutputDebugString(s,n);if(trace_file){fwrite(s,1,n,trace_file);fflush(trace_file);}}
static void gate(bool b,const char* label){if(!b)throw std::runtime_error(std::string("gate_failed: ")+label);trace(label);}
template<class F>static bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
struct Vertex{float p[3],color[4],uv[2];};
struct alignas(16) Constants{float matrix[16],tint[4];};
int main(){trace_file=fopen("sdmc:/fhnx_copy_trace.log","w");trace("boot");try{
 XenosCopyFrontend frontend;auto packet=xenosCopyFixture(),vertices=xenosCopyVertices();auto plan=frontend.decode(packet,0x100000,vertices);
 gate(plan.source.Pack()==4096 && plan.region.source_x==8 && plan.region.width==32,"xenos_copy_real_registers_vf0_plan_pass");
 Pm4Frontend draw_frontend;auto draw_bytes=indexedPm4Fixture(128,96,0x300000,6,0);std::memset(draw_bytes.data()+36,0,4);
 auto draw=draw_frontend.decode(draw_bytes);auto equal=indexedPm4Fixture(128,96,0x300000,6,0);equal[39]=0x26;
 gate(rejects([&]{draw_frontend.decode(equal);}),"pm4_equal_depth_contract_rejected");
 gate(rejects([&]{frontend.decode(packet,0x100000,xenosCopyVertices(9,8,40,40));}),"copy_partial_alignment_rejected");
 DekoBackend gpu(16*1024*1024);PinyonSurfaces surfaces(gpu,4*1024*1024);Pm4DrawExecutor executor(surfaces);
 auto vs=gpu.shader(buffer_vsh_bin,buffer_vsh_bin_size),fs=gpu.shader(pinyon_sample_bin,pinyon_sample_bin_size);
 Vertex quad[]={{{-1,-1,.2f},{1,1,1,1},{0,1}},{{1,-1,.2f},{1,1,1,1},{1,1}},{{1,1,.2f},{1,1,1,1},{1,0}},{{-1,1,.2f},{1,1,1,1},{0,0}}};
 uint16_t indices[]={0,1,2,0,2,3};auto vb=gpu.buffer(BufferKind::vertex,sizeof(quad),quad),ib=gpu.buffer(BufferKind::index16,sizeof(indices),indices);
 Constants identity{};for(unsigned i=0;i<4;++i)identity.matrix[i*5]=1;for(float& f:identity.tint)f=1;auto ub=gpu.buffer(BufferKind::uniform,sizeof(identity),&identity);
 executor.bindIndices(0x300000,ib,sizeof(indices),xenos::Endian::k8in16);executor.prepare(draw);
 auto overlapping=plan.source;overlapping.base_tiles=1;gate(rejects([&]{surfaces.acquire(overlapping,128,96);}),"copy_overlapping_edram_owner_rejected");

 VertexLayout layout{sizeof(Vertex),3,{{{0,3},{12,4},{28,2}}}};
 gpu.begin();surfaces.bind(plan.source);surfaces.clear(.25f,.5f,.75f,1);surfaces.transition(surfaces.get(plan.source),Usage::readback);gpu.submit();
 auto source=gpu.readback(surfaces.get(plan.source).image);for(unsigned y=0;y<96;++y)for(unsigned x=0;x<128;++x)require(source[y*128+x]==0xffbf8040u,"COPY source GPU clear reference mismatch");gate(source.size()==12288,"copy_source_gpu_clear_pixels_pass");
 XenosCopyDestination dest{plan.destination_address,gpu.create(plan.dest_desc),plan.dest_desc,Usage::undefined};
 auto mismatched=gpu.create({64,32});auto forged=dest;forged.image=mismatched;forged.usage=Usage::target;
 gate(rejects([&]{validateNativeCopy(gpu,surfaces,plan,forged);}),"copy_destination_metadata_rejected");gpu.destroy(mismatched);
 gate(rejects([&]{validateNativeCopy(gpu,surfaces,plan,dest);}),"copy_uninitialized_resource_state_rejected");
 gpu.begin();gpu.transition(dest.image,dest.usage,Usage::target);dest.usage=Usage::target;gpu.target(dest.image);gpu.clear(.25f,.125f,.5f,1);gpu.submit();
 auto feedback=dest;feedback.image=surfaces.get(plan.source).image;gate(rejects([&]{validateNativeCopy(gpu,surfaces,plan,feedback);}),"copy_resource_feedback_rejected");
 gpu.begin();issueNativeCopy(gpu,surfaces,plan,dest);gpu.transition(dest.image,dest.usage,Usage::readback);dest.usage=Usage::readback;gpu.submit();
 auto pixels=gpu.readback(dest.image);for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){bool inside=x>=8 && x<40 && y>=8 && y<40;auto expected=inside?0xffbf8040u:0xff802040u;require(pixels[y*64+x]==expected,"COPY region/outside pixel mismatch");}gate(pixels.size()==4096,"xenos_copy_all_4096_gpu_pixels_pass");
 std::vector<uint8_t> guest(64*64*4,0x5a);storeTiledCopy(plan,pixels,guest);
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){auto offset=texture_util::GetTiledOffset2D(x,y,64,2);bool inside=x>=8 && x<40 && y>=8 && y<40;for(unsigned b=0;b<4;++b)require(guest[offset+b]==(inside?uint8_t(0xffbf8040u>>(8*b)):0x5a),"COPY tiled guest byte/untouched mismatch");}gate(guest.size()==16384,"xenos_copy_tiled_guest_bytes_pass");
 if(auto f=fopen("sdmc:/fhnx_copy_gpu.ppm","wb")){fprintf(f,"P6\n64 64\n255\n");for(auto p:pixels){uint8_t rgb[]={uint8_t(p),uint8_t(p>>8),uint8_t(p>>16)};fwrite(rgb,1,3,f);}fclose(f);}
 if(auto f=fopen("sdmc:/fhnx_copy_tiled.bin","wb")){fwrite(guest.data(),1,guest.size(),f);fclose(f);}
 Image backs[]={gpu.create({960,540,1,Format::rgba8,true}),gpu.create({960,540,1,Format::rgba8,true})};Usage states[]={Usage::undefined,Usage::undefined};auto swap=gpu.swapchain(backs[0],backs[1]);
 PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);trace("resources_ready");unsigned frames=0;
 while(appletMainLoop()){padUpdate(&pad);auto down=padGetButtonsDown(&pad);if(down&HidNpadButton_A)trace("a_pressed");if(down&HidNpadButton_Plus){trace("plus_exit");break;}
  int slot=gpu.acquire(swap);gpu.begin();gpu.transition(backs[slot],states[slot],Usage::target);gpu.target(backs[slot]);gpu.vertex(vb,layout);gpu.uniform(ub);gpu.transition(dest.image,dest.usage,Usage::sampled);dest.usage=Usage::sampled;gpu.sample(dest.image,{Filter::nearest,Wrap::clamp});Draw present{vs,fs};present.indices=ib;present.count=6;gpu.draw(present);gpu.transition(backs[slot],Usage::target,Usage::present);gpu.submit();gpu.present(swap,slot,backs[slot]);states[slot]=Usage::present;if(++frames==2)trace("frames_submitted");}
 u64 ram=0;svcGetInfo(&ram,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);char line[256];snprintf(line,sizeof(line),"telemetry frames=%u images=%llu buffers=%llu process_used=%llu copy_pixels=4096 tiled_bytes=16384",frames,(unsigned long long)gpu.imageBytes(),(unsigned long long)gpu.bufferBytes(),(unsigned long long)ram);trace(line);
 gpu.destroySwapchain(swap);for(auto h:backs)gpu.destroy(h);surfaces.shutdown();gpu.destroy(dest.image);gpu.destroy(vb);gpu.destroy(ib);gpu.destroy(ub);gpu.destroy(vs);gpu.destroy(fs);gate(gpu.imageBytes()==0 && gpu.bufferBytes()==0,"copy_resources_reclaimed");
 }catch(const std::exception& e){trace(e.what());trace("copy_probe_failed");if(trace_file)fclose(trace_file);return 2;}trace("shutdown");if(trace_file)fclose(trace_file);return 0;}
