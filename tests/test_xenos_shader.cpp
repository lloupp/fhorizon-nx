#include "xenos_shader_fixture.hpp"
#include <fstream>
#include <cstdio>
#include <limits>
using namespace fhnx::gpu;using namespace rex::graphics;
static void check(bool b){require(b,"shader independent reference failed");}
template<class F>static void rejects(F f){bool yes=false;try{f();}catch(const std::runtime_error&){yes=true;}check(yes);}
static void change(std::vector<uint8_t>& b,unsigned word,uint32_t value){for(unsigned i=0;i<4;++i)b[word*4+i]=uint8_t(value>>(24-i*8));}
int main(int argc,char** argv){require(argc==2,"test output directory");XenosPixelShader translator;
 for(unsigned swapped=0;swapped<2;++swapped){auto bytes=syntheticPixelMicrocode(swapped);auto p=translator.decode(bytes,xenos::ShaderType::kPixel);
  check(bytes.size()==48 && p.operations.size()==3 && p.constant_mask==7);
  check(p.operations[0].opcode==ucode::AluVectorOpcode::kMul && p.operations[1].lhs.temporary && p.operations[1].rhs.index==2 && p.operations[2].export_color && p.operations[2].clamp);
  check(p.operations[0].lhs.components== (swapped?std::array<uint8_t,4>{1,0,2,3}:std::array<uint8_t,4>{0,1,2,3}));
  auto glsl=translator.glsl(p);check(glsl.find(swapped?"xe.c[0].yxzw":"xe.c[0].xyzw")!=std::string::npos);
  std::string base=std::string(argv[1])+"/pixel"+std::to_string(swapped);std::ofstream(base+".ucode",std::ios::binary).write(reinterpret_cast<char*>(bytes.data()),bytes.size());std::ofstream(base+".glsl")<<glsl;
 }
 auto bytes=syntheticPixelMicrocode();
 // Independent literal CF and ALU words, not the fixture enum builder.
 check(bytes[4]==0x30 && bytes[5]==0x01 && bytes[6]==0xc4 && bytes[7]==0 && bytes[8]==0x28);
 check(bytes[12]==0xc8 && bytes[13]==0x0f && bytes[20]==0x01 && bytes[22]==0x01);
 auto bad=[&](unsigned word,uint32_t value){auto b=bytes;change(b,word,value);rejects([&]{translator.decode(b,xenos::ShaderType::kPixel);});};
 bad(1,0x3001b400);bad(1,0x3001c600);bad(2,0x18000000);bad(1,0x4001c400);bad(1,0x3002c400);bad(1,0x3001c401);bad(1,0x3001c408); // branches, flags, bounds
 bad(5,0x0b000100);bad(5,0x80000100);bad(5,0x01000400); // MAD, undefined r0, c4
 bad(3,0xc80e0000);bad(3,0xc80f0040);bad(4,0x10000000);bad(4,0x80000000);bad(3,0); // mask, relative, predicate/address, scalar
 bad(4,0x04000000);bad(3,0xc80f0080); // negate / absolute modifiers unsupported
 bad(9,0xc90f8001); // unsupported color1 export
 bad(9,0xc90f0000); // missing export
 rejects([&]{translator.decode(bytes,xenos::ShaderType::kVertex);});
 for(unsigned length=0;length<bytes.size();++length)rejects([&]{translator.decode(std::span(bytes).first(length),xenos::ShaderType::kPixel);});
 auto trailing=bytes;trailing.resize(60);rejects([&]{translator.decode(trailing,xenos::ShaderType::kPixel);});
 auto forged=translator.decode(bytes,xenos::ShaderType::kPixel);forged.operations[0].lhs.index=4;rejects([&]{translator.glsl(forged);});
 XenosFloatConstants c{};validateXenosConstants(c);c.values[0][0]=std::numeric_limits<float>::infinity();rejects([&]{validateXenosConstants(c);});c.values[0][0]=17;rejects([&]{validateXenosConstants(c);});
 std::puts("PASS Xenos real CF/ALU decode, temps/constants, relative swizzle, color0 export, strict negative contracts");}
