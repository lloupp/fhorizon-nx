#pragma once
#include "xenos_shader.hpp"
namespace fhnx::gpu {
inline std::vector<uint8_t> syntheticPixelMicrocode(bool swap_rg=false){
 using namespace rex::graphics::ucode;
 // Real 48-bit CF pair: ALLOC colors(1), EXEC_END address1 count3 absolute.
 std::vector<uint32_t> words={0,0x3001c400,0x28000000};
 auto alu=[&](AluVectorOpcode opcode,uint32_t dest,bool export_color,bool clamp,uint32_t a,uint32_t b,bool a_temp,bool b_temp,uint32_t a_swizzle){
  words.push_back(dest|(uint32_t(export_color)<<15)|(15u<<16)|(uint32_t(clamp)<<24)|(uint32_t(AluScalarOpcode::kRetainPrev)<<26));
  words.push_back(a_swizzle<<16);
  words.push_back((a<<16)|(b<<8)|(uint32_t(opcode)<<24)|(uint32_t(a_temp)<<31)|(uint32_t(b_temp)<<30));
 };
 alu(AluVectorOpcode::kMul,0,false,false,0,1,false,false,swap_rg?0x0d:0);
 alu(AluVectorOpcode::kAdd,1,false,false,0,2,true,false,0);
 alu(AluVectorOpcode::kMax,0,true,true,1,1,true,true,0);
 std::vector<uint8_t> bytes;for(auto w:words)for(int s=24;s>=0;s-=8)bytes.push_back(uint8_t(w>>s));return bytes;
}
}
