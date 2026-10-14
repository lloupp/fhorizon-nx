#include "deko_backend.hpp"
#include "pm4_executor.hpp"
#include "pm4_fixture.hpp"
#include "xenos_shader_fixture.hpp"
#include "xenos_shader_bundle.hpp"
#include <switch.h>
#include <cstdio>
#include <cstring>
#include "buffer_vsh_bin.h"
#include "pinyon_sample_bin.h"
#include "xenos_pixel0_fsh_bin.h"
#include "xenos_pixel1_fsh_bin.h"
using namespace fhnx::gpu;using namespace rex::graphics;
extern "C" {u32 __nx_heap_size=64*1024*1024;}
static FILE* log_file;
static void trace(const char* text){char line[320];int n=snprintf(line,sizeof(line),"FHNX_GRAPHICS %s\n",text);svcOutputDebugString(line,n);if(log_file){fwrite(line,1,n,log_file);fflush(log_file);}}
static void gate(bool ok,const char* label){if(!ok)throw std::runtime_error(std::string("gate_failed: ")+label);trace(label);}
template<class F>static bool rejects(F f){try{f();}catch(const std::runtime_error&){return true;}return false;}
struct Vertex{float p[3],color[4],uv[2];};
struct alignas(16) Transform{float matrix[16],tint[4];};
static XenosFloatConstants constants(unsigned phase){XenosFloatConstants c{};c.values[0]={.5f,.25f,.75f,1};c.values[1]={1,.5f,.25f,1};
 if(phase%3==0)c.values[2]={.125f,.125f,.0625f,0};
 if(phase%3==2)c.values[2]={-.75f,.875f,1,0};
 return c;}
int main(){log_file=fopen("sdmc:/fhnx_shader_trace.log","w");trace("boot");try{
 XenosPixelShader decoder;auto primary=syntheticPixelMicrocode(),swapped=syntheticPixelMicrocode(true);
 auto p0=decoder.decode(primary,xenos::ShaderType::kPixel),p1=decoder.decode(swapped,xenos::ShaderType::kPixel);
 gate(std::equal(primary.begin(),primary.end(),bundle::program0.begin()) && std::equal(swapped.begin(),swapped.end(),bundle::program1.begin()) && decoder.glsl(p0)==bundle::glsl0 && decoder.glsl(p1)==bundle::glsl1,"xenos_microcode_glsl_bundle_identity_pass");
 auto unsupported=primary;unsupported[20]=0x0b;gate(rejects([&]{decoder.decode(unsupported,xenos::ShaderType::kPixel);}),"xenos_unsupported_mad_rejected");
 gate(rejects([&]{decoder.decode(primary,xenos::ShaderType::kVertex);}),"xenos_unimplemented_vertex_shader_rejected");
 auto relative=primary;relative[16]=0x80;gate(rejects([&]{decoder.decode(relative,xenos::ShaderType::kPixel);}),"xenos_relative_constants_rejected");
 DekoBackend gpu(16*1024*1024);PinyonSurfaces surfaces(gpu,4*1024*1024);Pm4Frontend frontend;Pm4DrawExecutor executor(surfaces);
 auto vs=gpu.shader(buffer_vsh_bin,buffer_vsh_bin_size),display=gpu.shader(pinyon_sample_bin,pinyon_sample_bin_size);
 Shader ps[]={gpu.shader(xenos_pixel0_fsh_bin,xenos_pixel0_fsh_bin_size),gpu.shader(xenos_pixel1_fsh_bin,xenos_pixel1_fsh_bin_size)};
 Vertex vertices[12]{};for(unsigned q=0;q<3;++q){float center=float(q)*.66f-.66f;float x[]={center-.22f,center+.22f,center+.22f,center-.22f},y[]={-.5f,-.5f,.5f,.5f};for(unsigned v=0;v<4;++v){auto& a=vertices[q*4+v];a.p[0]=x[v];a.p[1]=y[v];a.p[2]=.2f;for(float& f:a.color)f=1;}}
 auto vb=gpu.buffer(BufferKind::vertex,sizeof(vertices),vertices);Buffer ib[3];std::vector<Pm4Frontend::IndexedDraw> draws;
 for(unsigned q=0;q<3;++q){uint16_t indices[]={uint16_t(q*4),uint16_t(q*4+1),uint16_t(q*4+2),uint16_t(q*4),uint16_t(q*4+2),uint16_t(q*4+3)};
  ib[q]=gpu.buffer(BufferKind::index16,sizeof(indices),indices);auto packet=indexedPm4Fixture(320,180,0x1000+q*0x100,6,0);std::memset(packet.data()+36,0,4);auto draw=frontend.decode(packet)[0];draws.push_back(draw);executor.bindIndices(draw.address,ib[q],sizeof(indices),xenos::Endian::k8in16);}
 executor.prepare(draws);auto key=draws[0].color;
 Transform transform{};for(unsigned i=0;i<4;++i)transform.matrix[i*5]=1;for(float& f:transform.tint)f=1;
 auto u0=gpu.buffer(BufferKind::uniform,sizeof(transform),&transform);auto values=constants(0);validateXenosConstants(values);auto u1=gpu.buffer(BufferKind::uniform,sizeof(values),&values);
 VertexLayout layout{sizeof(Vertex),3,{{{0,3},{12,4},{28,2}}}};
 // Independent numeric references for MUL + ADD + clamped color export;
 // second shader swaps c0.rg via Xenos component-relative swizzle 0x0d.
 constexpr uint32_t reference[3][2]={{0xff40409f,0xff406060},{0xff302080,0xff304040},{0xffffff00,0xffffff00}};
 for(unsigned frame=0;frame<32;++frame){values=constants(frame);validateXenosConstants(values);gpu.update(u1,0,sizeof(values),&values);
  gpu.begin();surfaces.bind(key);surfaces.clear(.025f,.06f,.12f,1);gpu.vertex(vb,layout);gpu.uniform(u0,0);gpu.uniform(u1,1);
  for(unsigned q=0;q<3;++q){Draw pipeline{vs,ps[q==1?1:0]};executor.issue(draws[q],pipeline);}
  surfaces.transition(surfaces.get(key),Usage::readback);gpu.submit();auto pixels=gpu.readback(surfaces.get(key).image);
  unsigned x[]={54,160,265};for(unsigned q=0;q<3;++q){auto actual=pixels[90*320+x[q]],expected=reference[frame%3][q==1?1:0];bool good=true;
   for(unsigned channel=0;channel<4;++channel){int a=(actual>>(8*channel))&255,b=(expected>>(8*channel))&255;good&=std::abs(a-b)<=1;}
   char line[192];snprintf(line,sizeof(line),"xenos_shader_pixel frame=%u draw=%u program=%u actual=%08lx expected=%08lx pixels=%s",frame,q,q==1?1:0,(unsigned long)actual,(unsigned long)expected,good?"pass":"FAIL");trace(line);require(good,"Xenos translated pixel mismatch");}
 }
 trace("xenos_shader_32_frames_verified");
 Vertex screen[]={{{-1,-1,0},{1,1,1,1},{0,1}},{{1,-1,0},{1,1,1,1},{1,1}},{{1,1,0},{1,1,1,1},{1,0}},{{-1,1,0},{1,1,1,1},{0,0}}};uint16_t indices[]={0,1,2,0,2,3};auto svb=gpu.buffer(BufferKind::vertex,sizeof(screen),screen),sib=gpu.buffer(BufferKind::index16,sizeof(indices),indices);
 Image backs[]={gpu.create({960,540,1,Format::rgba8,true}),gpu.create({960,540,1,Format::rgba8,true})};Usage states[]={Usage::undefined,Usage::undefined};auto swap=gpu.swapchain(backs[0],backs[1]);PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);trace("resources_ready");unsigned frames=0;
 while(appletMainLoop()){padUpdate(&pad);auto down=padGetButtonsDown(&pad);if(down&HidNpadButton_A)trace("a_pressed");if(down&HidNpadButton_Plus){trace("plus_exit");break;}
  int slot=gpu.acquire(swap);gpu.begin();gpu.transition(backs[slot],states[slot],Usage::target);gpu.target(backs[slot]);gpu.clear(0,0,0,1);gpu.vertex(svb,layout);gpu.uniform(u0,0);surfaces.sample(key);Draw d{vs,display};d.indices=sib;d.count=6;gpu.draw(d);gpu.transition(backs[slot],Usage::target,Usage::present);gpu.submit();gpu.present(swap,slot,backs[slot]);states[slot]=Usage::present;if(++frames==2)trace("frames_submitted");}
 u64 memory=0;svcGetInfo(&memory,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);char line[192];snprintf(line,sizeof(line),"telemetry process_used=%llu images=%llu buffers=%llu frames=%u shader_frames=32",(unsigned long long)memory,(unsigned long long)gpu.imageBytes(),(unsigned long long)gpu.bufferBytes(),frames);trace(line);
 gpu.destroySwapchain(swap);for(auto h:backs)gpu.destroy(h);surfaces.shutdown();gpu.destroy(vb);for(auto h:ib)gpu.destroy(h);gpu.destroy(u0);gpu.destroy(u1);gpu.destroy(svb);gpu.destroy(sib);gpu.destroy(vs);gpu.destroy(display);for(auto h:ps)gpu.destroy(h);gate(gpu.bufferBytes()==0 && gpu.imageBytes()==0,"xenos_shader_resources_reclaimed");
 }catch(const std::exception& e){trace(e.what());trace("xenos_shader_probe_failed");if(log_file)fclose(log_file);return 2;}trace("shutdown");if(log_file)fclose(log_file);return 0;}
