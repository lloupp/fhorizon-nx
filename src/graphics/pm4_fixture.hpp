#pragma once
#include "pm4_frontend.hpp"

namespace fhnx::gpu {
// Original synthetic packets, not captured title commands. Values are built
// with upstream enums/registers and encoded as guest big-endian bytes.
inline std::vector<uint8_t> indexedPm4Fixture(uint32_t width,uint32_t height,
                                            uint32_t address,uint32_t count,uint32_t msaa=2) {
    using namespace rex::graphics;
    std::vector<uint8_t> b;
    auto word=[&](uint32_t w){for(int shift=24;shift>=0;shift-=8)b.push_back(uint8_t(w>>shift));};
    auto reg=[&](uint32_t r,uint32_t v){word(xenos::MakePacketType0(r,1));word(v);};
    reg(XE_GPU_REG_RB_SURFACE_INFO,width|(msaa<<16));
    reg(XE_GPU_REG_RB_COLOR_INFO,0);reg(XE_GPU_REG_RB_DEPTH_INFO,512);
    reg::RB_DEPTHCONTROL depth_control{};depth_control.z_enable=1;depth_control.z_write_enable=1;
    depth_control.zfunc=xenos::CompareFunction::kLess;
    reg(XE_GPU_REG_RB_COLOR_MASK,15);reg(XE_GPU_REG_RB_DEPTHCONTROL,depth_control.value);
    reg(XE_GPU_REG_RB_MODECONTROL,uint32_t(xenos::EdramMode::kColorDepth));
    reg(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL,0x80000000u);
    reg(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR,width|(height<<16));
    word(xenos::MakePacketType0(XE_GPU_REG_PA_CL_VPORT_XSCALE,6));
    for(float f:{float(width)/2,float(width)/2,float(height)/2,float(height)/2,1.f,0.f})word(std::bit_cast<uint32_t>(f));
    reg::VGT_DRAW_INITIATOR d{};d.prim_type=xenos::PrimitiveType::kTriangleList;
    d.source_select=xenos::SourceSelect::kDMA;d.num_indices=count;
    reg::VGT_DMA_SIZE size{};size.num_words=count;size.swap_mode=xenos::Endian::k8in16;
    word(xenos::MakePacketType3(xenos::PM4_DRAW_INDX,4));
    word(0);word(d.value);word(address);word(size.value);
    return b;
}
}
