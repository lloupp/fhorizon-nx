#include "horizon_memory.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

namespace fhnx {
void HorizonMemory::success(Result rc,const char* what) {
    if(R_FAILED(rc)) throw std::runtime_error(std::string(what)+" rc="+std::to_string(rc));
}
HorizonMemory::HorizonMemory(size_t budget) : budget_(budget) {
    if(!budget || budget%page_size) throw std::runtime_error("physical budget alignment");
}
void HorizonMemory::range(size_t offset,size_t bytes) const {
    if(!base_ || offset>size_ || bytes>size_-offset) throw std::runtime_error("virtual range");
}
void HorizonMemory::reserve(size_t bytes) {
    if(base_ || !bytes || bytes%page_size) throw std::runtime_error("reservation size/state");
    u64 stack_size=0; success(svcGetInfo(&stack_size,InfoType_StackRegionSize,CUR_PROCESS_HANDLE,0),"stack region");
    if(bytes>stack_size || stack_size-bytes<2*page_size) throw std::runtime_error("reservation exceeds Horizon stack region");
    virtmemLock();
    auto* candidate=static_cast<uint8_t*>(virtmemFindStack(bytes,page_size));
    auto* reservation=candidate ? virtmemAddReservation(candidate,bytes) : nullptr;
    virtmemUnlock();
    if(!reservation) throw std::runtime_error("virtual reservation failed");
    base_=candidate; reservation_=reservation; size_=bytes;
}
void HorizonMemory::commit(size_t offset) {
    range(offset,page_size);
    if(offset%page_size || pages_.contains(offset)) throw std::runtime_error("commit alignment/state");
    if(page_size>budget_-committed()) throw std::runtime_error("commit budget exceeded");
    void* source=std::aligned_alloc(page_size,page_size);
    if(!source) throw std::bad_alloc();
    std::memset(source,0,page_size);
    // Allocate metadata before SVC so allocation failure cannot strand a mapping.
    try { pages_.emplace(offset,Page{source,Protection::read_write}); }
    catch(...) {std::free(source); throw;}
    Result rc=svcMapMemory(base_+offset,source,page_size);
    if(R_FAILED(rc)) {pages_.erase(offset); std::free(source); success(rc,"map heap page");}
}
void HorizonMemory::decommit(size_t offset) {
    range(offset,page_size); auto it=pages_.find(offset);
    if(it==pages_.end()) throw std::runtime_error("decommit state");
    // Unmap restores the source heap page. Free only after confirmed success.
    success(svcUnmapMemory(base_+offset,it->second.source,page_size),"unmap heap page");
    std::free(it->second.source); pages_.erase(it);
}
Result HorizonMemory::protect(size_t offset,Protection p) {
    range(offset,page_size); auto it=pages_.find(offset);
    if(it==pages_.end()) throw std::runtime_error("protect uncommitted page");
    u32 perm=0;
    switch(p) {case Protection::none:perm=Perm_None;break;case Protection::read:perm=Perm_R;break;case Protection::read_write:perm=Perm_Rw;break;default:throw std::runtime_error("unsupported protection");}
    Result rc=svcSetMemoryPermission(base_+offset,page_size,perm);
    if(R_SUCCEEDED(rc)) it->second.protection=p;
    return rc;
}
void HorizonMemory::access(size_t offset,size_t bytes,bool write) const {
    range(offset,bytes);
    for(size_t p=offset&~(page_size-1); p<offset+bytes; p+=page_size) {
        auto it=pages_.find(p);
        if(it==pages_.end() || it->second.protection==Protection::none || (write && it->second.protection!=Protection::read_write))
            throw std::runtime_error("guest access denied");
    }
}
void HorizonMemory::store32be(size_t offset,uint32_t v) {
    access(offset,4,true); for(unsigned i=0;i<4;++i) base_[offset+i]=uint8_t(v>>(24-8*i));
}
uint32_t HorizonMemory::load32be(size_t offset) const {
    access(offset,4,false); uint32_t v=0; for(unsigned i=0;i<4;++i) v=(v<<8)|base_[offset+i]; return v;
}
HorizonMemory::~HorizonMemory() {
    while(!pages_.empty()) {
        try {decommit(pages_.begin()->first);} catch(...) {
            // A failed unmap must never free inaccessible borrowed source pages.
            svcBreak(BreakReason_Panic,0,0); std::abort();
        }
    }
    if(reservation_) {virtmemLock();virtmemRemoveReservation(reservation_);virtmemUnlock();}
}
}
