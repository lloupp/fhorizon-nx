#pragma once
// Bounded decoder of the pinned ShiftGlue CommandProcessor wire contract.
// This is not the desktop command processor, guest MMU, or shader translator.
#include "pinyon_surfaces.hpp"
#include <rex/graphics/registers.h>
#include <bit>
#include <span>
#include <vector>

namespace fhnx::gpu {
class Pm4Frontend {
    using Key = rex::graphics::Fh1SurfaceKey;
    std::map<uint32_t,uint32_t> values_;
    static bool supported(uint32_t r) {
        using namespace rex::graphics;
        switch (r) {
        case XE_GPU_REG_RB_SURFACE_INFO: case XE_GPU_REG_RB_COLOR_INFO:
        case XE_GPU_REG_RB_DEPTH_INFO: case XE_GPU_REG_RB_DEPTHCONTROL:
        case XE_GPU_REG_RB_COLOR_MASK: case XE_GPU_REG_RB_MODECONTROL:
        case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL: case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR:
        case XE_GPU_REG_PA_CL_VPORT_XSCALE: case XE_GPU_REG_PA_CL_VPORT_XOFFSET:
        case XE_GPU_REG_PA_CL_VPORT_YSCALE: case XE_GPU_REG_PA_CL_VPORT_YOFFSET:
        case XE_GPU_REG_PA_CL_VPORT_ZSCALE: case XE_GPU_REG_PA_CL_VPORT_ZOFFSET:
            return true;
        default: return false;
        }
    }
    uint32_t read(uint32_t r) const {
        auto it=values_.find(r);
        require(it!=values_.end(), "PM4 missing required register");
        return it->second;
    }
public:
    struct IndexedDraw {
        Key color, depth;
        uint32_t width, height, address, index_bytes, count;
        rex::graphics::xenos::Endian endian;
        bool depth_test, depth_write;
    };
private:
    IndexedDraw draw(std::span<const uint32_t> p) const {
        using namespace rex::graphics;
        require(p.size() == 4 && p[0] == 0, "PM4 DRAW_INDX size or conditional draw unsupported");
        reg::VGT_DRAW_INITIATOR init{p[1]}; reg::VGT_DMA_SIZE dma{p[3]};
        require(init.prim_type == xenos::PrimitiveType::kTriangleList &&
                init.source_select == xenos::SourceSelect::kDMA &&
                init.index_size == xenos::IndexFormat::kInt16 &&
                init.major_mode == xenos::MajorMode::kImplicit &&
                !init.not_eop && !(p[1] & 0xe400u), "PM4 unsupported draw initiator");
        require(init.num_indices && init.num_indices % 3 == 0 &&
                dma.num_words >= init.num_indices && !(p[3] & 0x3f000000u) &&
                !(p[2] & 1), "PM4 index range or alignment");
        require(dma.swap_mode == xenos::Endian::kNone || dma.swap_mode == xenos::Endian::k8in16,
                "PM4 index endian unsupported");
        require(read(XE_GPU_REG_RB_MODECONTROL) == uint32_t(xenos::EdramMode::kColorDepth),
                "PM4 EDRAM mode unsupported (copy requires region derivation)");
        require(read(XE_GPU_REG_RB_COLOR_MASK) == 15, "PM4 MRT or color mask unsupported");
        reg::RB_SURFACE_INFO surface{read(XE_GPU_REG_RB_SURFACE_INFO)};
        reg::RB_COLOR_INFO color{read(XE_GPU_REG_RB_COLOR_INFO)};
        reg::RB_DEPTH_INFO depth{read(XE_GPU_REG_RB_DEPTH_INFO)};
        uint32_t z = read(XE_GPU_REG_RB_DEPTHCONTROL);
        reg::RB_DEPTHCONTROL supported_depth{};
        supported_depth.z_enable=1;supported_depth.z_write_enable=1;
        supported_depth.zfunc=xenos::CompareFunction::kLess;
        require(z == 0 || z == supported_depth.value, "PM4 depth/stencil state unsupported");
        reg::PA_SC_WINDOW_SCISSOR_BR br{read(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR)};
        require(read(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL) == 0x80000000u &&
                !(br.value & 0xc000c000u) && br.br_x == surface.surface_pitch && br.br_y,
                "PM4 only full-target scissor without window offset supported");
        require(!(surface.value & 0xfffcC000u) &&
                !(color.value & ~0x7ffu) && !(depth.value & ~0x7ffu),
                "PM4 unsupported surface format or flags");
        const uint32_t w = br.br_x, h = br.br_y;
        auto f = [&](uint32_t r){return std::bit_cast<float>(read(r));};
        require(f(XE_GPU_REG_PA_CL_VPORT_XSCALE) == float(w)/2 &&
                f(XE_GPU_REG_PA_CL_VPORT_XOFFSET) == float(w)/2 &&
                f(XE_GPU_REG_PA_CL_VPORT_YSCALE) == float(h)/2 &&
                f(XE_GPU_REG_PA_CL_VPORT_YOFFSET) == float(h)/2 &&
                f(XE_GPU_REG_PA_CL_VPORT_ZSCALE) == 1 &&
                f(XE_GPU_REG_PA_CL_VPORT_ZOFFSET) == 0, "PM4 unsupported viewport");
        uint32_t msaa = uint32_t(surface.msaa_samples), pitch = Fh1PitchTiles(w, msaa);
        auto ck = Key::Color(color.color_base, pitch, msaa, color.color_format, false);
        auto zk = Key::Depth(depth.depth_base, pitch, msaa, depth.depth_format);
        PinyonSurfaces::describe(ck,w,h); PinyonSurfaces::describe(zk,w,h);
        return {ck,zk,w,h,p[2],uint32_t(dma.num_words)*2,init.num_indices,dma.swap_mode,z!=0,z!=0};
    }
public:
    // Validate the entire batch before returning any executable draws. On error
    // retain the previous register state and emit no backend calls.
    std::vector<IndexedDraw> decode(std::span<const uint8_t> bytes) {
        require(bytes.size()%4 == 0 && bytes.size() <= 1024*1024, "PM4 stream size");
        Pm4Frontend next = *this;
        std::vector<IndexedDraw> result;
        size_t cursor=0;
        auto word=[&](){auto p=bytes.subspan(cursor,4);cursor+=4;
            return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];};
        while(cursor<bytes.size()) {
            uint32_t header=word(), type=header>>30;
            if(type==2){require(header==0x80000000u,"PM4 invalid Type 2");continue;}
            require(type==0 || type==3,"PM4 Type 1 unsupported");
            uint32_t count=((header>>16)&0x3fff)+1;
            require(count <= (bytes.size()-cursor)/4,"PM4 truncated payload");
            if(type==0) {
                uint32_t base=header&0x7fff;bool one=header&0x8000;
                for(uint32_t i=0;i<count;++i) {
                    uint32_t r=base+(one?0:i);
                    require(supported(r),"PM4 register unsupported");
                    next.values_[r]=word();
                }
            } else {
                using namespace rex::graphics::xenos;
                require(!(header&0x80ffu),"PM4 predicate or reserved bits unsupported");
                uint32_t opcode=(header>>8)&0x7f;
                require(opcode==PM4_NOP || opcode==PM4_DRAW_INDX,"PM4 opcode unsupported");
                if(opcode==PM4_NOP){cursor+=count*4;continue;}
                require(count==4,"PM4 indexed DMA payload size");
                std::array<uint32_t,4> p;for(auto& v:p)v=word();
                result.push_back(next.draw(p));
            }
        }
        *this=next;return result;
    }
};
}
