#pragma once
#include "pinyon_surfaces.hpp"
#include <rex/graphics/registers.h>
#include "fhnx_upstream_tiling.hpp"
#include <bit>
#include <cmath>
#include <span>
#include <algorithm>

namespace fhnx::gpu {
// Actual COPY mode contract from ShiftGlue GetResolveInfo / Fh1ResolveRectangle.
// Bounded: RGBA8 raw 1x, already-aligned integer rectangles, no clipping,
// window offsets, conversion, clear, array slices, swap or endian conversion.
struct XenosCopyPlan {
    PinyonSurfaces::Key source;
    ImageDesc source_desc,dest_desc;
    uint32_t destination_address;
    CopyRegion region;
};
class XenosCopyFrontend {
    static bool supported(uint32_t r){using namespace rex::graphics;
        switch(r){case XE_GPU_REG_RB_MODECONTROL:case XE_GPU_REG_RB_SURFACE_INFO:case XE_GPU_REG_RB_COLOR_INFO:
        case XE_GPU_REG_RB_COPY_CONTROL:case XE_GPU_REG_RB_COPY_DEST_BASE:case XE_GPU_REG_RB_COPY_DEST_INFO:case XE_GPU_REG_RB_COPY_DEST_PITCH:
        case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL:case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR:case XE_GPU_REG_PA_SC_SCREEN_SCISSOR_TL:case XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR:
        case XE_GPU_REG_PA_SC_WINDOW_OFFSET:case XE_GPU_REG_PA_SU_SC_MODE_CNTL:case XE_GPU_REG_PA_SU_VTX_CNTL:
        case XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0:case XE_GPU_REG_SHADER_CONSTANT_FETCH_00_1:return true;default:return false;}}
public:
    XenosCopyPlan decode(std::span<const uint8_t> bytes,uint32_t vertex_address,std::span<const uint8_t> vertices) const {
        using namespace rex::graphics;require(bytes.size()%4==0 && bytes.size()<=4096,"COPY stream size");
        std::map<uint32_t,uint32_t> registers;size_t cursor=0;bool copied=false;
        auto word=[&](){require(cursor+4<=bytes.size(),"COPY truncated word");auto p=bytes.subspan(cursor,4);cursor+=4;return(uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];};
        while(cursor<bytes.size()){
            auto header=word();uint32_t type=header>>30;
            if(type==2){require(header==0x80000000u,"COPY invalid NOP");continue;}
            require(type==0 || type==3,"COPY packet type unsupported");uint32_t count=((header>>16)&0x3fff)+1;
            require(count<=(bytes.size()-cursor)/4,"COPY truncated packet");
            if(type==0){uint32_t base=header&0x7fff;bool one=header&0x8000;for(uint32_t i=0;i<count;++i){auto r=base+(one?0:i);require(supported(r),"COPY register unsupported");registers[r]=word();}}
            else {require(!(header&0x80ffu),"COPY predicated/reserved packet unsupported");uint32_t opcode=(header>>8)&0x7f;
                if(opcode==xenos::PM4_NOP){cursor+=count*4;continue;}
                require(opcode==xenos::PM4_DRAW_INDX && count==2,"COPY requires real auto-index DRAW_INDX");require(word()==0,"COPY conditional draw unsupported");
                reg::VGT_DRAW_INITIATOR init{word()};reg::VGT_DRAW_INITIATOR expected{};expected.prim_type=xenos::PrimitiveType::kRectangleList;expected.source_select=xenos::SourceSelect::kAutoIndex;expected.num_indices=3;
                require(init.value==expected.value && cursor==bytes.size(),"COPY draw initiator or trailing commands unsupported");copied=true;}}
        require(copied,"COPY missing draw");auto read=[&](uint32_t r){auto it=registers.find(r);require(it!=registers.end(),"COPY required register missing");return it->second;};
        require(read(XE_GPU_REG_RB_MODECONTROL)==uint32_t(xenos::EdramMode::kCopy),"COPY mode required");
        reg::RB_COPY_CONTROL control{read(XE_GPU_REG_RB_COPY_CONTROL)};require(control.value==0,"COPY supports raw target0 sample0, no clear only");
        reg::RB_SURFACE_INFO surface{read(XE_GPU_REG_RB_SURFACE_INFO)};require(surface.value==surface.surface_pitch && surface.surface_pitch,"COPY MSAA/flags unsupported");
        reg::RB_COLOR_INFO color{read(XE_GPU_REG_RB_COLOR_INFO)};require(!(color.value&~0x7ffu),"COPY color format/flags unsupported");
        reg::RB_COPY_DEST_INFO info{read(XE_GPU_REG_RB_COPY_DEST_INFO)};require(info.value==(uint32_t(xenos::ColorFormat::k_8_8_8_8)<<7),"COPY destination format/endian/array/swap unsupported");
        reg::RB_COPY_DEST_PITCH pitch{read(XE_GPU_REG_RB_COPY_DEST_PITCH)};
        require(!(pitch.value&0xc000c000u) && pitch.copy_dest_pitch && pitch.copy_dest_height && pitch.copy_dest_pitch<=512 && pitch.copy_dest_height<=512 && pitch.copy_dest_pitch%32==0 && pitch.copy_dest_height%32==0,"COPY destination pitch/height unsupported");
        uint32_t destination=read(XE_GPU_REG_RB_COPY_DEST_BASE);require(destination && !(destination&4095u) && uint64_t(destination)+uint64_t(pitch.copy_dest_pitch)*pitch.copy_dest_height*4<=uint64_t(UINT32_MAX)+1,"COPY destination address/alignment");
        require(read(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL)==0x80000000u && read(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_TL)==0 && read(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR)==0x20002000u &&
                read(XE_GPU_REG_PA_SC_WINDOW_OFFSET)==0 && read(XE_GPU_REG_PA_SU_SC_MODE_CNTL)==0 && read(XE_GPU_REG_PA_SU_VTX_CNTL)==0,"COPY clipping/window/pixel-center subset");
        reg::PA_SC_WINDOW_SCISSOR_BR br{read(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR)};
        require(!(br.value&0xc000c000u) && br.br_x==surface.surface_pitch && br.br_y,"COPY source full scissor");
        xenos::xe_gpu_vertex_fetch_t fetch{};fetch.dword_0=read(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0);fetch.dword_1=read(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_1);
        require(fetch.type==xenos::FetchConstantType::kVertex && fetch.size==6 && fetch.endian==xenos::Endian::k8in32 && !(fetch.dword_1&0xfc000000u) && uint64_t(fetch.address)*4==vertex_address && vertices.size()==24,"COPY VF0 binding/size/endian");
        require(uint64_t(vertex_address)+vertices.size()<=destination || uint64_t(destination)+uint64_t(pitch.copy_dest_pitch)*pitch.copy_dest_height*4<=vertex_address,"COPY destination overlaps vertex memory");
        uint32_t v[6];for(size_t i=0;i<6;++i){uint32_t bits=0;for(size_t j=0;j<4;++j)bits=(bits<<8)|vertices[i*4+j];float f=std::bit_cast<float>(bits);
            require(std::isfinite(f) && f>=0 && f<=4096 && std::floor(f)==f && uint32_t(f)%8==0,"COPY only prealigned integer VF0 rectangles supported");v[i]=uint32_t(f);}
        // At integer coordinates the upstream +0.5 D3D offset, fixed16p8
        // rounding and +127 top-left rule give exactly these integer bounds.
        uint32_t x0=std::min({v[0],v[2],v[4]}),y0=std::min({v[1],v[3],v[5]}),x1=std::max({v[0],v[2],v[4]}),y1=std::max({v[1],v[3],v[5]});
        require(x1>x0 && y1>y0,"COPY empty rectangle");
        ImageDesc source{br.br_x,br.br_y},dest{pitch.copy_dest_pitch,pitch.copy_dest_height};CopyRegion region{x0,y0,x0,y0,x1-x0,y1-y0};validateCopyRegion(source,dest,region);
        auto key=Fh1SurfaceKey::Color(color.color_base,Fh1PitchTiles(source.width,0),0,xenos::ColorRenderTargetFormat::k_8_8_8_8,false);PinyonSurfaces::describe(key,source.width,source.height);
        require(key.base_tiles+((source.height+15)/16)*key.pitch_tiles<=xenos::kEdramTileCount,"COPY EDRAM wrap unsupported");
        return {key,source,dest,destination,region};
    }
};
struct XenosCopyDestination {uint32_t address;Image image;ImageDesc desc;Usage usage;};
inline void validateNativeCopy(const Commands& gpu,PinyonSurfaces& surfaces,const XenosCopyPlan& plan,const XenosCopyDestination& dest){
    auto& source=surfaces.get(plan.source);
    require(source.desc.width==plan.source_desc.width && source.desc.height==plan.source_desc.height,"COPY source geometry binding");
    require(source.image.slot!=dest.image.slot && dest.image.slot!=UINT32_MAX,"COPY source/destination feedback");
    require(dest.address==plan.destination_address && dest.desc.width==plan.dest_desc.width && dest.desc.height==plan.dest_desc.height,"COPY destination address/geometry binding");
    auto actual=gpu.imageDesc(dest.image);
    require(actual.width==dest.desc.width && actual.height==dest.desc.height && actual.samples==dest.desc.samples && actual.format==dest.desc.format,"COPY destination metadata mismatch");
    require((source.usage==Usage::target || source.usage==Usage::readback) && (dest.usage==Usage::target || dest.usage==Usage::readback),"COPY initialized resource states required");
    validateCopyRegion(source.desc,dest.desc,plan.region);
}
inline void issueNativeCopy(Commands& gpu,PinyonSurfaces& surfaces,const XenosCopyPlan& plan,XenosCopyDestination& dest){
    validateNativeCopy(gpu,surfaces,plan,dest);auto& source=surfaces.get(plan.source);
    surfaces.transition(source,Usage::copy_source);gpu.transition(dest.image,dest.usage,Usage::copy_dest);dest.usage=Usage::copy_dest;
    gpu.copyRegion(source.image,dest.image,plan.region);
}
// Materialize the copied rectangle in a bounded synthetic guest memory view,
// using upstream's actual Xenos tiled address function. Input is GPU readback.
inline void storeTiledCopy(const XenosCopyPlan& p,std::span<const uint32_t> gpu_pixels,std::span<uint8_t> guest) {
    require(gpu_pixels.size()==size_t(p.dest_desc.width)*p.dest_desc.height && guest.size()>=gpu_pixels.size()*4,"COPY readback/tiled memory size");
    auto r=p.region;for(uint32_t y=r.dest_y;y<r.dest_y+r.height;++y)for(uint32_t x=r.dest_x;x<r.dest_x+r.width;++x){
        auto offset=rex::graphics::texture_util::GetTiledOffset2D(x,y,p.dest_desc.width,2);require(offset>=0 && size_t(offset)+4<=guest.size(),"COPY tiled offset bounds");
        auto pixel=gpu_pixels[y*p.dest_desc.width+x];for(unsigned b=0;b<4;++b)guest[offset+b]=uint8_t(pixel>>(8*b));}
}
}
