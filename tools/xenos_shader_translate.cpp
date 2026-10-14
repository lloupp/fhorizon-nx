#include "xenos_shader_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
using namespace fhnx::gpu;
int main(int argc,char** argv){try{
 require(argc==3,"usage: xenos_shader_translate microcode.be output.glsl | --synthetic output-directory");
 XenosPixelShader translator;
 if(std::string(argv[1])=="--synthetic"){
  std::filesystem::create_directories(argv[2]);
  for(unsigned i=0;i<2;++i){auto bytes=syntheticPixelMicrocode(i);auto p=translator.decode(bytes,rex::graphics::xenos::ShaderType::kPixel);
   std::string base=std::string(argv[2])+"/xenos_pixel"+std::to_string(i);
   std::ofstream binary(base+".ucode.be",std::ios::binary);binary.write(reinterpret_cast<char*>(bytes.data()),bytes.size());require(bool(binary),"synthetic microcode write failed");
   std::ofstream output(base+"_fsh.glsl");output<<translator.glsl(p);require(bool(output),"synthetic GLSL write failed");
  }
 }else {
  std::ifstream input(argv[1],std::ios::binary);require(bool(input),"microcode input open failed");
  input.seekg(0,std::ios::end);auto size=input.tellg();require(size>=24 && size<=84 && size%12==0,"microcode file exceeds bounded shader contract");input.seekg(0);
  std::vector<uint8_t> bytes(size_t(size),uint8_t{});input.read(reinterpret_cast<char*>(bytes.data()),size);require(bool(input),"microcode read failed");
  auto p=translator.decode(bytes,rex::graphics::xenos::ShaderType::kPixel);
  std::ofstream out(argv[2]);out<<translator.glsl(p);require(bool(out),"GLSL output write failed");
  std::printf("Xenos pixel subset: ALU=%zu constant_mask=%x binding=1\n",p.operations.size(),p.constant_mask);
 }
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}return 0;}
