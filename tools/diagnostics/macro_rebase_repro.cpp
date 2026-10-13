#include "ankerl/unordered_dense.h"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
int main(int argc,char**) {
 ankerl::unordered_dense::map<uint32_t,std::vector<uint32_t>> code;
 code[0]={1,2,3,4,5,6,7,8};
 if(argc>1) {
  const auto& source=code.at(0);
  std::vector<uint32_t> rebased(source.begin()+1,source.end());
  code.emplace(1,std::move(rebased));
 } else {
  const auto& source=code.at(0);
  auto& destination=code[1];
  destination.resize(source.size()-1);
  std::memcpy(destination.data(),source.data()+1,destination.size()*sizeof(uint32_t));
 }
 std::printf("REBASE_PASS words=%zu first=%u\n",code.at(1).size(),code.at(1).at(0));
 return code.at(1)==std::vector<uint32_t>{2,3,4,5,6,7,8}?0:2;
}
