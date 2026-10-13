#include "pm4_fixture.hpp"
#include "pm4_executor.hpp"
#include <cstdio>
using namespace fhnx::gpu;
static void check(bool b){if(!b)throw std::runtime_error("PM4 reference mismatch");}
template<class F>static void rejects(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected);}
static void put(std::vector<uint8_t>& b,size_t offset,uint32_t w){for(int i=0;i<4;++i)b.at(offset+i)=uint8_t(w>>(24-i*8));}
int main(){
 const uint8_t indices[]={0x12,0x34,0xab,0xcd};
 check(Pm4DrawExecutor::decodeIndex16(indices,rex::graphics::xenos::Endian::k8in16)==std::vector<uint16_t>({0x1234,0xabcd}));
 check(Pm4DrawExecutor::decodeIndex16(indices,rex::graphics::xenos::Endian::kNone)==std::vector<uint16_t>({0x3412,0xcdab}));
 rejects([&]{Pm4DrawExecutor::decodeIndex16(std::span(indices,3),rex::graphics::xenos::Endian::kNone);});
 Pm4Frontend decoder;auto b=indexedPm4Fixture(320,180,0x1000,6);
 auto draws=decoder.decode(b);check(draws.size()==1);auto d=draws[0];
 // Independent numeric references to the documented Xenos packet/fields.
 check(b[b.size()-20]==0xc0 && b[b.size()-18]==0x22);
 check(d.color.Pack()==(8u<<11 | 2u<<19));
 check(d.depth.Pack()==(512u | 8u<<11 | 2u<<19 | 1u<<21));
 check(d.width==320 && d.height==180 && d.address==0x1000 && d.count==6 && d.index_bytes==12);
 check(uint32_t(d.endian)==1 && d.depth_test && d.depth_write);
 for(size_t n=1;n<20;++n){auto c=b;c.resize(c.size()-n);rejects([&]{decoder.decode(c);});}
 auto negative=[&](size_t offset,uint32_t value){auto c=b;put(c,offset,value);rejects([&]{decoder.decode(c);});};
 negative(b.size()-20,0xc0037f00); // unknown Type 3 opcode
 negative(b.size()-20,0xc0032201); // predicate
 negative(b.size()-16,1); // conditional query
 negative(b.size()-12,0x00060804); // 32-bit indices
 negative(b.size()-8,0x1001);negative(b.size()-4,5u|(1u<<30));
 negative(12,0x10000); // unsupported color format
 negative(4,320u|(1u<<16)); // 2x MSAA
 negative(44,6); // COPY mode is not silently treated as a normal draw
 negative(60,319u|(180u<<16)); // partial scissor
 negative(0,0x7fff); // unknown register
 std::vector<uint8_t> type1={0x40,0,0,0,0,0,0,0,0,0,0,0};rejects([&]{decoder.decode(type1);});
 // Failed batch includes a valid state mutation followed by an unknown opcode.
 auto c=b;put(c,12,0x10000);put(c,c.size()-20,0xc0037f00);rejects([&]{decoder.decode(c);});
 std::vector<uint8_t> onlyDraw(b.end()-20,b.end());check(decoder.decode(onlyDraw)[0].color.format==0);
 Pm4Frontend empty;rejects([&]{empty.decode(onlyDraw);});
 // Type 0 write-one-register consumes all values; only the final value applies.
 auto repeat=b;put(repeat,0,0x00018000u|0x2000);repeat.insert(repeat.begin()+8,4,0);put(repeat,8,320u|(2u<<16));check(decoder.decode(repeat).size()==1);
 std::puts("PM4 PASS: BE wire, real register unions, indexed DMA, transactional rejection");
}
