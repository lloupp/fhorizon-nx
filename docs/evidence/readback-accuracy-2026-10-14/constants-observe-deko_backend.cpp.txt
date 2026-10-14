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
    for(auto& p:programs_) if(p.memory) dkMemBlockDestroy(p.memory);
    for(auto& b:buffers_) if(b.memory) dkMemBlockDestroy(b.memory);
    if(readback_memory_)dkMemBlockDestroy(readback_memory_);
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
    require(!recording_ && data && bytes >= 16 && bytes <= 64*1024,"shader budget/header");
    uint32_t slot=0;while(slot<programs_.size() && programs_[slot].memory)++slot;
    require(slot<16,"shader slot budget");
    require(std::memcmp(data,"DKSH",4)==0,"expected DKSH, not SPIR-V");
    Program p; p.memory=block(device_,align(bytes,DK_SHADER_CODE_ALIGNMENT),DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code);
    std::memcpy(dkMemBlockGetCpuAddr(p.memory),data,bytes);
    DkShaderMaker m; dkShaderMakerDefaults(&m,p.memory,0); dkShaderInitialize(&p.shader,&m);
    p.stage=dkShaderGetStage(&p.shader);
    if(p.stage != DkStage_Vertex && p.stage != DkStage_Fragment) { dkMemBlockDestroy(p.memory); throw std::runtime_error("shader stage"); }
    try {
        if(slot==programs_.size())programs_.push_back(p);
        else {p.generation=programs_[slot].generation;programs_[slot]=p;}
    } catch (...) { dkMemBlockDestroy(p.memory); throw; }
    return {slot,p.generation};
}
void DekoBackend::recording() { require(recording_ && ++command_count_ <= 128,"bounded command batch"); }
DkImageView DekoBackend::view(Image h) { images_.get(h); DkImageView v; dkImageViewDefaults(&v,&resources_[h.slot].image); return v; }
void DekoBackend::begin() {
    require(!dkQueueIsInErrorState(queue_),"deko queue in error before begin");
    require(!recording_,"nested begin"); dkQueueWaitIdle(queue_); dkCmdBufClear(commands_);
    dkCmdBufAddMemory(commands_,command_memory_,0,256*1024);
    color_={}; depth_={}; sampled_={}; vertex_buffer_={};vertex_count_=0;descriptor_cursor_=0;command_count_=0; recording_=true;
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
void DekoBackend::sample(Image h,Sampler settings) {
    recording(); auto& e=images_.get(h); require(e.usage==Usage::sampled,"sample state");
    require(descriptor_cursor_<16,"descriptor arena exhausted");
    require((settings.filter==Filter::nearest || settings.filter==Filter::linear) && (settings.wrap==Wrap::repeat || settings.wrap==Wrap::clamp),"sampler settings");
    uint32_t id=descriptor_cursor_++;
    sampled_=h;
    auto* cpu=static_cast<uint8_t*>(dkMemBlockGetCpuAddr(descriptors_));
    auto v=view(h); dkImageDescriptorInitialize(reinterpret_cast<DkImageDescriptor*>(cpu+id*sizeof(DkImageDescriptor)),&v,false,false);
    DkSampler sampler; dkSamplerDefaults(&sampler);
    sampler.minFilter=sampler.magFilter=settings.filter==Filter::nearest?DkFilter_Nearest:DkFilter_Linear;
    sampler.wrapMode[0]=sampler.wrapMode[1]=sampler.wrapMode[2]=settings.wrap==Wrap::repeat?DkWrapMode_Repeat:DkWrapMode_ClampToEdge;
    dkSamplerDescriptorInitialize(reinterpret_cast<DkSamplerDescriptor*>(cpu+2048+id*sizeof(DkSamplerDescriptor)),&sampler);
    dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_Descriptors);
    dkCmdBufBindImageDescriptorSet(commands_,dkMemBlockGetGpuAddr(descriptors_),16);
    dkCmdBufBindSamplerDescriptorSet(commands_,dkMemBlockGetGpuAddr(descriptors_)+2048,16);
    auto texture=dkMakeTextureHandle(id,id); dkCmdBufBindTextures(commands_,DkStage_Fragment,0,&texture,1);
}
void DekoBackend::draw(const Draw& d) {
    recording(); images_.targets(color_,depth_);
    require(d.count && d.count<=65536 && d.first<=65536-d.count,"draw range");
    require(d.vertex.slot<programs_.size() && d.fragment.slot<programs_.size(),"shader handle");
    require(programs_[d.vertex.slot].memory && programs_[d.fragment.slot].memory && programs_[d.vertex.slot].generation==d.vertex.generation && programs_[d.fragment.slot].generation==d.fragment.generation,"stale shader handle");
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
    require(d.depth_compare==DepthCompare::native_default || d.depth_compare==DepthCompare::less,"unsupported depth compare");
    if(d.depth_compare==DepthCompare::less)ds.depthCompareOp=DkCompareOp_Less;
    ds.depthTestEnable=d.depth_test; ds.depthWriteEnable=d.depth_write;
    ds.stencilTestEnable=d.stencil!=Stencil::disabled;
    if(d.stencil==Stencil::equal) {
        ds.stencilFrontCompareOp=ds.stencilBackCompareOp=DkCompareOp_Equal;
        ds.stencilFrontPassOp=ds.stencilBackPassOp=DkStencilOp_Keep;
    }
    dkCmdBufBindDepthStencilState(commands_,&ds);
    dkCmdBufSetStencil(commands_,DkFace_FrontAndBack,0xff,d.reference,0xff);
    if(d.indices.slot!=UINT32_MAX) {
        auto& ib=storage(d.indices);require(ib.kind==BufferKind::index16 && vertex_buffer_.slot!=UINT32_MAX,"indexed buffer kind/vertex binding");
        require(d.first<=ib.size/2 && d.count<=ib.size/2-d.first,"index range");
        auto* values=static_cast<uint16_t*>(dkMemBlockGetCpuAddr(ib.memory));
        for(uint32_t i=0;i<d.count;++i){int64_t index=int64_t(values[d.first+i])+d.vertex_offset;require(index>=0 && index<vertex_count_,"index exceeds vertex buffer");}
        dkCmdBufBindIdxBuffer(commands_,DkIdxFormat_Uint16,dkMemBlockGetGpuAddr(ib.memory));
        dkCmdBufDrawIndexed(commands_,DkPrimitive_Triangles,d.count,1,d.first,d.vertex_offset,0);
    } else {
        if(vertex_buffer_.slot!=UINT32_MAX)require(d.first<=vertex_count_ && d.count<=vertex_count_-d.first,"vertex range");
        dkCmdBufDraw(commands_,DkPrimitive_Triangles,d.count,1,d.first,0);
    }
}
void DekoBackend::resolve(Image s, Image d) {
    recording(); images_.resolve(s,d); auto sv=view(s),dv=view(d);
    dkCmdBufResolveImage(commands_,&sv,&dv);
}
void DekoBackend::submit() {
    require(recording_,"submit without begin");
    dkQueueSubmitCommands(queue_,dkCmdBufFinishList(commands_)); dkQueueWaitIdle(queue_); recording_=false;
    require(!dkQueueIsInErrorState(queue_),"deko queue in error after submit/wait");
}
void DekoBackend::copy(Image s,Image d) {
    recording(); auto& src=images_.get(s);auto& dst=images_.get(d);
    require(src.usage==Usage::copy_source && dst.usage==Usage::copy_dest,"copy states");
    require(src.desc.format==Format::rgba8 && dst.desc.format==Format::rgba8 && src.desc.samples==1 && dst.desc.samples==1,"copy format/samples");
    require(src.desc.width==dst.desc.width && src.desc.height==dst.desc.height && s.slot!=d.slot,"copy dimensions/feedback");
    auto sv=view(s),dv=view(d);DkImageRect rect={0,0,0,src.desc.width,src.desc.height,1};
    dkCmdBufCopyImage(commands_,&sv,&rect,&dv,&rect,0);
}
void DekoBackend::copyRegion(Image s,Image d,CopyRegion r) {
    recording();auto& src=images_.get(s);auto& dst=images_.get(d);
    require(src.usage==Usage::copy_source && dst.usage==Usage::copy_dest && s.slot!=d.slot,"region copy states/feedback");
    validateCopyRegion(src.desc,dst.desc,r);auto sv=view(s),dv=view(d);
    DkImageRect from={r.source_x,r.source_y,0,r.width,r.height,1},to={r.dest_x,r.dest_y,0,r.width,r.height,1};
    dkCmdBufCopyImage(commands_,&sv,&from,&dv,&to,0);
}
std::vector<uint32_t> DekoBackend::readback(Image h) {
    require(!recording_ && images_.get(h).usage==Usage::readback,"readback state");
    auto d=images_.get(h).desc; std::vector<uint32_t> pixels(size_t(d.width)*d.height);
    uint32_t bytes=uint32_t(pixels.size()*4);
    if(bytes>readback_capacity_){dkQueueWaitIdle(queue_);if(readback_memory_)dkMemBlockDestroy(readback_memory_);readback_memory_=nullptr;readback_capacity_=0;
        readback_memory_=block(device_,bytes,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);readback_capacity_=bytes;}
    auto mem=readback_memory_;
    begin(); auto v=view(h); DkImageRect rect={0,0,0,d.width,d.height,1}; DkCopyBuf copy={dkMemBlockGetGpuAddr(mem),0,0};
    dkCmdBufCopyImageToBuffer(commands_,&v,&rect,&copy,0);
    dkCmdBufBarrier(commands_,DkBarrier_Full,0); submit();
    std::memcpy(pixels.data(),dkMemBlockGetCpuAddr(mem),pixels.size()*4);
    return pixels;
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
DekoBackend::Storage& DekoBackend::storage(Buffer h) {
    require(h.slot<buffers_.size(),"buffer slot");auto& b=buffers_[h.slot];
    require(b.memory && b.generation==h.generation,"stale buffer handle");return b;
}
Buffer DekoBackend::buffer(BufferKind kind,uint32_t size,const void* data) {
    require(!recording_ && size && size<=4*1024*1024,"buffer size/state");
    require(kind==BufferKind::vertex || kind==BufferKind::index16 || kind==BufferKind::uniform,"buffer kind");
    require(kind!=BufferKind::index16 || size%2==0,"index alignment");
    require(kind!=BufferKind::uniform || size<=65536,"uniform size");
    uint32_t padded=align(size,DK_MEMBLOCK_ALIGNMENT);
    require(padded<=8*1024*1024-buffer_bytes_,"buffer budget exceeded");
    for(uint32_t i=0;i<buffers_.size();++i)if(!buffers_[i].memory){
        auto& b=buffers_[i];b.memory=block(device_,padded,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);b.size=size;b.kind=kind;
        std::memset(dkMemBlockGetCpuAddr(b.memory),0,padded);
        if(data)std::memcpy(dkMemBlockGetCpuAddr(b.memory),data,size);
        buffer_bytes_+=padded;return {i,b.generation};
    }
    throw std::runtime_error("buffer slots exhausted");
}
void DekoBackend::update(Buffer h,uint32_t offset,uint32_t bytes,const void* data) {
    require(!recording_ && data,"buffer update during recording/null source");auto& b=storage(h);
    require(offset<=b.size && bytes<=b.size-offset,"buffer update bounds");
    dkQueueWaitIdle(queue_);std::memcpy(static_cast<uint8_t*>(dkMemBlockGetCpuAddr(b.memory))+offset,data,bytes);
}
void DekoBackend::destroy(Buffer h) {
    require(!recording_,"buffer destroy during recording");auto& b=storage(h);dkQueueWaitIdle(queue_);
    dkMemBlockDestroy(b.memory);buffer_bytes_-=align(b.size,DK_MEMBLOCK_ALIGNMENT);b.memory=nullptr;++b.generation;
}
void DekoBackend::destroy(Shader h) {
    require(!recording_ && h.slot<programs_.size(),"shader destroy state");auto& p=programs_[h.slot];
    require(p.memory && p.generation==h.generation,"stale shader handle");dkQueueWaitIdle(queue_);
    dkMemBlockDestroy(p.memory);p.memory=nullptr;++p.generation;
}
void DekoBackend::vertex(Buffer h,const VertexLayout& layout) {
    recording();auto& b=storage(h);require(b.kind==BufferKind::vertex,"vertex buffer kind");
    require(layout.stride && layout.stride<=2048 && layout.count && layout.count<=8 && b.size%layout.stride==0,"vertex layout");
    std::array<DkVtxAttribState,8> attributes{};
    const DkVtxAttribSize sizes[]={DkVtxAttribSize_1x32,DkVtxAttribSize_2x32,DkVtxAttribSize_3x32,DkVtxAttribSize_4x32};
    for(uint32_t i=0;i<layout.count;++i){auto a=layout.attributes[i];require(a.components>=1 && a.components<=4 && a.offset%4==0 && a.offset<=layout.stride && a.components*4<=layout.stride-a.offset,"vertex attribute");attributes[i].bufferId=0;attributes[i].offset=a.offset;attributes[i].size=sizes[a.components-1];attributes[i].type=DkVtxAttribType_Float;}
    DkVtxBufferState state={layout.stride,0};DkBufExtents ext={dkMemBlockGetGpuAddr(b.memory),b.size};
    dkCmdBufBindVtxAttribState(commands_,attributes.data(),layout.count);dkCmdBufBindVtxBufferState(commands_,&state,1);dkCmdBufBindVtxBuffers(commands_,0,&ext,1);
    vertex_buffer_=h;vertex_count_=b.size/layout.stride;
}
void DekoBackend::uniform(Buffer h,uint32_t binding) {
    recording();auto& b=storage(h);require(b.kind==BufferKind::uniform && binding<8,"uniform binding");
    // Snapshot constants into the GPU command stream. Their backing allocation
    // is reused only after submit/wait, and each bind records its own values.
    dkCmdBufPushConstants(commands_,dkMemBlockGetGpuAddr(b.memory),align(b.size,DK_UNIFORM_BUF_ALIGNMENT),0,b.size,dkMemBlockGetCpuAddr(b.memory));
    dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_Shader);
    dkCmdBufBindUniformBuffer(commands_,DkStage_Vertex,binding,dkMemBlockGetGpuAddr(b.memory),align(b.size,DK_UNIFORM_BUF_ALIGNMENT));
    dkCmdBufBindUniformBuffer(commands_,DkStage_Fragment,binding,dkMemBlockGetGpuAddr(b.memory),align(b.size,DK_UNIFORM_BUF_ALIGNMENT));
}
void DekoBackend::uploadRGBA8(Image h,const void* data,uint32_t bytes) {
    require(!recording_ && data,"upload state/data");auto d=images_.get(h).desc;
    require(d.format==Format::rgba8 && d.samples==1 && bytes==uint64_t(d.width)*d.height*4,"upload dimensions/format");
    auto before=images_.get(h).usage;auto staging=block(device_,bytes,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
    std::memcpy(dkMemBlockGetCpuAddr(staging),data,bytes);
    begin();transition(h,before,Usage::copy_dest);auto v=view(h);DkImageRect rect={0,0,0,d.width,d.height,1};DkCopyBuf source={dkMemBlockGetGpuAddr(staging),0,0};
    dkCmdBufCopyBufferToImage(commands_,&source,&v,&rect,0);transition(h,Usage::copy_dest,Usage::sampled);submit();dkMemBlockDestroy(staging);
}
}
