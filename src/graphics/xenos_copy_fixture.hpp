#pragma once
#include "xenos_copy.hpp"
namespace fhnx::gpu {
inline std::vector<uint8_t> xenosCopyFixture(uint32_t width=128,uint32_t height=96,uint32_t destination=0x200000,uint32_t vertex_address=0x100000){
    using namespace rex::graphics;std::vector<uint8_t> b;
    auto word=[&](uint32_t w){for(int i=24;i>=0;i-=8)b.push_back(uint8_t(w>>i));};
    auto reg=[&](Register r,uint32_t v){word(xenos::MakePacketType0(r,1));word(v);};
    reg(XE_GPU_REG_RB_MODECONTROL,uint32_t(xenos::EdramMode::kCopy));reg(XE_GPU_REG_RB_SURFACE_INFO,width);reg(XE_GPU_REG_RB_COLOR_INFO,0);
    reg(XE_GPU_REG_RB_COPY_CONTROL,0);reg(XE_GPU_REG_RB_COPY_DEST_BASE,destination);
    reg::RB_COPY_DEST_INFO info{};info.copy_dest_format=xenos::ColorFormat::k_8_8_8_8;reg(XE_GPU_REG_RB_COPY_DEST_INFO,info.value);
    reg::RB_COPY_DEST_PITCH pitch{};pitch.copy_dest_pitch=pitch.copy_dest_height=64;reg(XE_GPU_REG_RB_COPY_DEST_PITCH,pitch.value);
    reg(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL,0x80000000u);reg(XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR,width|(height<<16));
    reg(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_TL,0);reg(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR,0x20002000u);
    reg(XE_GPU_REG_PA_SC_WINDOW_OFFSET,0);reg(XE_GPU_REG_PA_SU_SC_MODE_CNTL,0);reg(XE_GPU_REG_PA_SU_VTX_CNTL,0);
    xenos::xe_gpu_vertex_fetch_t fetch{};fetch.type=xenos::FetchConstantType::kVertex;fetch.address=vertex_address/4;fetch.size=6;fetch.endian=xenos::Endian::k8in32;
    reg(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0,fetch.dword_0);reg(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_1,fetch.dword_1);
    reg::VGT_DRAW_INITIATOR draw{};draw.prim_type=xenos::PrimitiveType::kRectangleList;draw.source_select=xenos::SourceSelect::kAutoIndex;draw.num_indices=3;
    word(xenos::MakePacketType3(xenos::PM4_DRAW_INDX,2));word(0);word(draw.value);return b;
}
inline std::vector<uint8_t> xenosCopyVertices(float x0=8,float y0=8,float x1=40,float y1=40){
    std::vector<uint8_t> b;for(float f:{x0,y0,x1,y0,x0,y1}){auto word=std::bit_cast<uint32_t>(f);for(int i=24;i>=0;i-=8)b.push_back(uint8_t(word>>i));}return b;
}
}
