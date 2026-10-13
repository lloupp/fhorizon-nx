#include "deko_backend.hpp"
#include <cstring>
#include <switch.h>

namespace fhnx::gpu {
static uint32_t align(uint32_t n, uint32_t a) { return (n+a-1)&~(a-1); }
static DkMemBlock block(DkDevice d, uint32_t n, uint32_t flags) {
    DkMemBlockMaker m; dkMemBlockMakerDefaults(&m,d,align(n,DK_MEMBLOCK_ALIGNMENT)); m.flags=flags;
    auto b=dkMemBlockCreate(&m); require(b != nullptr,"deko allocation failed"); return b;
}
DekoBackend::DekoBackend(uint64_t budget) : images_(budget) {
    // All owned objects are unwound on partial initialization failure.
    try {
        DkDeviceMaker dm; dkDeviceMakerDefaults(&dm); device_=dkDeviceCreate(&dm); require(device_,"deko device");
        DkQueueMaker qm; dkQueueMakerDefaults(&qm,device_); qm.flags=DkQueueFlags_Graphics;
        queue_=dkQueueCreate(&qm); require(queue_,"deko queue");
        command_memory_=block(device_,256*1024,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
        descriptors_=block(device_,DK_MEMBLOCK_ALIGNMENT,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
        DkCmdBufMaker cm; dkCmdBufMakerDefaults(&cm,device_); commands_=dkCmdBufCreate(&cm); require(commands_,"deko commands");
    } catch (...) {
        if(commands_) dkCmdBufDestroy(commands_);
        if(descriptors_) dkMemBlockDestroy(descriptors_);
        if(command_memory_) dkMemBlockDestroy(command_memory_);
        if(queue_) dkQueueDestroy(queue_);
        if(device_) dkDeviceDestroy(device_);
        throw;
    }
}
DekoBackend::~DekoBackend() {
    dkQueueWaitIdle(queue_);
    if(swap_) dkSwapchainDestroy(swap_);
    dkCmdBufDestroy(commands_);
    for(auto& p:programs_) dkMemBlockDestroy(p.memory);
    for(auto& r:resources_) if(r.memory) dkMemBlockDestroy(r.memory);
    dkMemBlockDestroy(descriptors_); dkMemBlockDestroy(command_memory_);
    dkQueueDestroy(queue_); dkDeviceDestroy(device_);
}
Image DekoBackend::create(ImageDesc d) {
    require(!recording_,"create during recording"); Images::validate(d);
    DkImageLayoutMaker m; dkImageLayoutMakerDefaults(&m,device_);
    m.format=d.format == Format::rgba8 ? DkImageFormat_RGBA8_Unorm : DkImageFormat_Z24S8;
    m.flags=DkImageFlags_UsageRender|DkImageFlags_Usage2DEngine;
    if(d.present) m.flags|=DkImageFlags_UsagePresent;
    if(d.samples == 4) { m.type=DkImageType_2DMS; m.msMode=DkMsMode_4x; }
    m.dimensions[0]=d.width; m.dimensions[1]=d.height;
    DkImageLayout layout; dkImageLayoutInitialize(&layout,&m);
    auto bytes=align(dkImageLayoutGetSize(&layout),DK_MEMBLOCK_ALIGNMENT);
    auto h=images_.insert(d,bytes);
    try {
        auto& r=resources_[h.slot]; r.memory=block(device_,bytes,DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image);
        dkImageInitialize(&r.image,&layout,r.memory,0);
    } catch (...) { images_.erase(h); throw; }
    return h;
}
void DekoBackend::destroy(Image h) {
    require(!recording_,"destroy during recording"); images_.get(h); dkQueueWaitIdle(queue_);
    require(!swap_ || (h.slot!=swap_images_[0].slot && h.slot!=swap_images_[1].slot),"destroy image owned by swapchain");
    dkMemBlockDestroy(resources_[h.slot].memory); resources_[h.slot]={}; images_.erase(h);
}
Shader DekoBackend::shader(const void* data, uint32_t bytes) {
    require(!recording_ && data && bytes >= 16 && bytes <= 64*1024 && programs_.size()<16,"shader budget/header");
    require(std::memcmp(data,"DKSH",4)==0,"expected DKSH, not SPIR-V");
    Program p; p.memory=block(device_,align(bytes,DK_SHADER_CODE_ALIGNMENT),DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code);
    std::memcpy(dkMemBlockGetCpuAddr(p.memory),data,bytes);
    DkShaderMaker m; dkShaderMakerDefaults(&m,p.memory,0); dkShaderInitialize(&p.shader,&m);
    p.stage=dkShaderGetStage(&p.shader);
    if(p.stage != DkStage_Vertex && p.stage != DkStage_Fragment) { dkMemBlockDestroy(p.memory); throw std::runtime_error("shader stage"); }
    try { programs_.push_back(p); } catch (...) { dkMemBlockDestroy(p.memory); throw; }
    return {uint32_t(programs_.size()-1)};
}
void DekoBackend::recording() { require(recording_ && ++command_count_ <= 128,"bounded command batch"); }
DkImageView DekoBackend::view(Image h) { images_.get(h); DkImageView v; dkImageViewDefaults(&v,&resources_[h.slot].image); return v; }
void DekoBackend::begin() {
    require(!recording_,"nested begin"); dkQueueWaitIdle(queue_); dkCmdBufClear(commands_);
    dkCmdBufAddMemory(commands_,command_memory_,0,256*1024);
    color_={}; depth_={}; sampled_={}; command_count_=0; recording_=true;
}
void DekoBackend::transition(Image h, Usage before, Usage after) {
    recording(); images_.transition(h,before,after);
    // deko has no Vulkan layout object. Explicit GPU ordering and texture invalidation
    // implement the use transition; full barriers are conservative, not optimized.
    dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_Image);
}
void DekoBackend::target(Image c, Image z) {
    recording(); images_.targets(c,z); color_=c; depth_=z;
    auto cv=view(c); auto zv=z.slot == UINT32_MAX ? DkImageView{} : view(z);
    dkCmdBufBindRenderTarget(commands_,&cv,z.slot == UINT32_MAX ? nullptr : &zv);
    const auto& d=images_.get(c).desc;
    DkViewport vp={0,0,float(d.width),float(d.height),0,1}; DkScissor sc={0,0,d.width,d.height};
    dkCmdBufSetViewports(commands_,0,&vp,1); dkCmdBufSetScissors(commands_,0,&sc,1);
    DkMultisampleState ms; dkMultisampleStateDefaults(&ms);
    if(d.samples==4) ms.mode=ms.rasterizerMode=DkMsMode_4x;
    dkCmdBufBindMultisampleState(commands_,&ms);
}
void DekoBackend::clear(float r,float g,float b,float a,bool depth,float z,uint8_t stencil) {
    recording(); images_.targets(color_,depth_);
    require(!depth || depth_.slot != UINT32_MAX,"clear without depth attachment");
    dkCmdBufClearColorFloat(commands_,0,DkColorMask_RGBA,r,g,b,a);
    if(depth) dkCmdBufClearDepthStencil(commands_,true,z,0xff,stencil);
}
void DekoBackend::sample(Image h) {
    recording(); auto& e=images_.get(h); require(e.usage==Usage::sampled,"sample state");
    // One sampled binding per submitted batch: its descriptor storage cannot be
    // mutated between recorded draws. A future descriptor arena must lift this.
    require(command_count_ && sampled_.slot == UINT32_MAX,"one texture per batch");
    sampled_=h;
    auto v=view(h); dkImageDescriptorInitialize(static_cast<DkImageDescriptor*>(dkMemBlockGetCpuAddr(descriptors_)),&v,false,false);
    DkSampler sampler; dkSamplerDefaults(&sampler);
    dkSamplerDescriptorInitialize(reinterpret_cast<DkSamplerDescriptor*>(static_cast<uint8_t*>(dkMemBlockGetCpuAddr(descriptors_))+128),&sampler);
    dkCmdBufBindImageDescriptorSet(commands_,dkMemBlockGetGpuAddr(descriptors_),1);
    dkCmdBufBindSamplerDescriptorSet(commands_,dkMemBlockGetGpuAddr(descriptors_)+128,1);
    auto texture=dkMakeTextureHandle(0,0); dkCmdBufBindTextures(commands_,DkStage_Fragment,0,&texture,1);
}
void DekoBackend::draw(const Draw& d) {
    recording(); images_.targets(color_,depth_);
    require(d.count && d.count<=65536 && d.first<=65536-d.count,"draw range");
    require(d.vertex.slot<programs_.size() && d.fragment.slot<programs_.size(),"shader handle");
    require(programs_[d.vertex.slot].stage==DkStage_Vertex && programs_[d.fragment.slot].stage==DkStage_Fragment,"pipeline shader stages");
    require((!d.depth_test && !d.depth_write && d.stencil==Stencil::disabled) || depth_.slot != UINT32_MAX,"draw without depth/stencil");
    require(d.stencil==Stencil::disabled || d.stencil==Stencil::replace || d.stencil==Stencil::equal,"unsupported stencil state");
    if(sampled_.slot != UINT32_MAX) require(images_.get(sampled_).usage==Usage::sampled && sampled_.slot!=color_.slot,"texture feedback/state");
    const DkShader* shaders[]={&programs_[d.vertex.slot].shader,&programs_[d.fragment.slot].shader};
    dkCmdBufBindShaders(commands_,DkStageFlag_GraphicsMask,shaders,2);
    DkRasterizerState raster; dkRasterizerStateDefaults(&raster); dkCmdBufBindRasterizerState(commands_,&raster);
    DkColorState color; dkColorStateDefaults(&color); dkCmdBufBindColorState(commands_,&color);
    DkColorWriteState write; dkColorWriteStateDefaults(&write); if(!d.color_write) dkColorWriteStateSetMask(&write,0,0);
    dkCmdBufBindColorWriteState(commands_,&write);
    DkDepthStencilState ds; dkDepthStencilStateDefaults(&ds);
    ds.depthTestEnable=d.depth_test; ds.depthWriteEnable=d.depth_write;
    ds.stencilTestEnable=d.stencil!=Stencil::disabled;
    if(d.stencil==Stencil::equal) {
        ds.stencilFrontCompareOp=ds.stencilBackCompareOp=DkCompareOp_Equal;
        ds.stencilFrontPassOp=ds.stencilBackPassOp=DkStencilOp_Keep;
    }
    dkCmdBufBindDepthStencilState(commands_,&ds);
    dkCmdBufSetStencil(commands_,DkFace_FrontAndBack,0xff,d.reference,0xff);
    dkCmdBufDraw(commands_,DkPrimitive_Triangles,d.count,1,d.first,0);
}
void DekoBackend::resolve(Image s, Image d) {
    recording(); images_.resolve(s,d); auto sv=view(s),dv=view(d);
    dkCmdBufResolveImage(commands_,&sv,&dv);
}
void DekoBackend::submit() {
    require(recording_,"submit without begin");
    dkQueueSubmitCommands(queue_,dkCmdBufFinishList(commands_)); dkQueueWaitIdle(queue_); recording_=false;
}
void DekoBackend::copy(Image s,Image d) {
    recording(); auto& src=images_.get(s);auto& dst=images_.get(d);
    require(src.usage==Usage::copy_source && dst.usage==Usage::copy_dest,"copy states");
    require(src.desc.format==Format::rgba8 && dst.desc.format==Format::rgba8 && src.desc.samples==1 && dst.desc.samples==1,"copy format/samples");
    require(src.desc.width==dst.desc.width && src.desc.height==dst.desc.height && s.slot!=d.slot,"copy dimensions/feedback");
    auto sv=view(s),dv=view(d);DkImageRect rect={0,0,0,src.desc.width,src.desc.height,1};
    dkCmdBufCopyImage(commands_,&sv,&rect,&dv,&rect,0);
}
std::vector<uint32_t> DekoBackend::readback(Image h) {
    require(!recording_ && images_.get(h).usage==Usage::readback,"readback state");
    auto d=images_.get(h).desc; std::vector<uint32_t> pixels(size_t(d.width)*d.height);
    auto mem=block(device_,uint32_t(pixels.size()*4),DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
    begin(); auto v=view(h); DkImageRect rect={0,0,0,d.width,d.height,1}; DkCopyBuf copy={dkMemBlockGetGpuAddr(mem),0,0};
    dkCmdBufCopyImageToBuffer(commands_,&v,&rect,&copy,0);
    dkCmdBufBarrier(commands_,DkBarrier_Full,0); submit();
    std::memcpy(pixels.data(),dkMemBlockGetCpuAddr(mem),pixels.size()*4); dkMemBlockDestroy(mem); return pixels;
}
DkSwapchain DekoBackend::swapchain(Image a,Image b) {
    require(!recording_ && !swap_,"one swapchain, outside recording");
    auto da=images_.get(a).desc,db=images_.get(b).desc;
    require(da.present && db.present && a.slot != b.slot && da.width==db.width && da.height==db.height,"swapchain images");
    const DkImage* targets[]={&resources_[a.slot].image,&resources_[b.slot].image};
    DkSwapchainMaker m; dkSwapchainMakerDefaults(&m,device_,nwindowGetDefault(),targets,2);
    auto s=dkSwapchainCreate(&m); require(s,"swapchain allocation"); swap_=s; swap_images_[0]=a; swap_images_[1]=b; return s;
}
int DekoBackend::acquire(DkSwapchain s) { require(!recording_ && s && s==swap_,"acquire swapchain"); return dkQueueAcquireImage(queue_,s); }
void DekoBackend::present(DkSwapchain s,int slot,Image h) {
    require(!recording_ && s && s==swap_ && slot>=0 && slot<2 && swap_images_[slot].slot==h.slot && images_.get(h).usage==Usage::present,"present state"); dkQueuePresentImage(queue_,s,slot);
}
void DekoBackend::destroySwapchain(DkSwapchain s) { require(!recording_ && s && s==swap_,"destroy swapchain"); dkQueueWaitIdle(queue_); dkSwapchainDestroy(s); swap_=nullptr; }
}
