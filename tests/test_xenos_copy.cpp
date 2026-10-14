#include "xenos_copy_fixture.hpp"
#include <cstdio>
#include <set>
using namespace fhnx::gpu;using namespace rex::graphics;
static void check(bool b){require(b,"COPY independent reference failed");}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::runtime_error&){yes=true;}check(yes);}
static uint32_t word(std::span<const uint8_t> b){return(uint32_t(b[0])<<24)|(uint32_t(b[1])<<16)|(uint32_t(b[2])<<8)|b[3];}
static void replace(std::vector<uint8_t>& b,Register reg,uint32_t value){for(size_t i=0;i+8<=b.size();i+=8)if(word(std::span(b).subspan(i,4))==uint32_t(reg)){for(unsigned j=0;j<4;++j)b[i+4+j]=uint8_t(value>>(24-j*8));return;}throw std::runtime_error("test register absent");}
int main(){XenosCopyFrontend decoder;auto commands=xenosCopyFixture(),vertices=xenosCopyVertices();auto p=decoder.decode(commands,0x100000,vertices);
 check(p.source.Pack()==4096 && p.source_desc.width==128 && p.source_desc.height==96 && p.destination_address==0x200000 && p.dest_desc.width==64 && p.dest_desc.height==64);
 check(p.region.source_x==8 && p.region.source_y==8 && p.region.dest_x==8 && p.region.dest_y==8 && p.region.width==32 && p.region.height==32);
 check(word(std::span(commands).last(4))==0x00030088); // 3 indices, auto, rectangle
 auto negative=[&](Register r,uint32_t value){auto c=commands;replace(c,r,value);rejects([&]{decoder.decode(c,0x100000,vertices);});};
 negative(XE_GPU_REG_RB_COPY_DEST_INFO,0);negative(XE_GPU_REG_RB_COPY_DEST_PITCH,63|(64u<<16));negative(XE_GPU_REG_RB_COPY_DEST_BASE,0x200004);
 negative(XE_GPU_REG_RB_COPY_CONTROL,1u<<20);negative(XE_GPU_REG_RB_COPY_CONTROL,1u<<8);negative(XE_GPU_REG_RB_COLOR_INFO,1u<<16);
 negative(XE_GPU_REG_RB_SURFACE_INFO,128|(2u<<16));negative(XE_GPU_REG_PA_SU_SC_MODE_CNTL,1);negative(XE_GPU_REG_RB_COPY_DEST_BASE,0x100000);
 rejects([&]{decoder.decode(commands,0x100004,vertices);});rejects([&]{decoder.decode(commands,0x100000,std::span(vertices).first(20));});
 for(unsigned n=1;n<12;++n){auto c=commands;c.resize(c.size()-n);rejects([&]{decoder.decode(c,0x100000,vertices);});}
 for(auto v:{xenosCopyVertices(9,8,40,40),xenosCopyVertices(8,8,136,40),xenosCopyVertices(8,8,72,40),xenosCopyVertices(8,8,8,40),xenosCopyVertices(-8,8,40,40)})rejects([&]{decoder.decode(commands,0x100000,v);});
 using rex::graphics::texture_util::GetTiledOffset2D;
 check(GetTiledOffset2D(0,0,64,2)==0 && GetTiledOffset2D(1,0,64,2)==4 && GetTiledOffset2D(0,1,64,2)==16 && GetTiledOffset2D(32,0,64,2)==4096 && GetTiledOffset2D(0,32,64,2)==8192);
 std::set<int32_t> all;for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto offset=GetTiledOffset2D(x,y,64,2);check(offset>=0 && offset<16384 && offset%4==0);all.insert(offset);}check(all.size()==4096);
 std::vector<uint32_t> pixels(4096,0xff402010);std::vector<uint8_t> guest(16384,0x5a);storeTiledCopy(p,pixels,guest);
 unsigned changed=0;for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){auto offset=GetTiledOffset2D(x,y,64,2);bool inside=x>=8 && x<40 && y>=8 && y<40;for(unsigned b=0;b<4;++b){auto expected=inside?uint8_t(0xff402010u>>(8*b)):0x5a;check(guest[offset+b]==expected);changed+=inside;}}check(changed==4096);
 rejects([&]{storeTiledCopy(p,pixels,std::span(guest).first(10));});
 std::puts("COPY PASS: real COPY/VF0/auto DRAW_INDX, region bounds, format/pitch/overlap negatives, upstream tiled layout");}
