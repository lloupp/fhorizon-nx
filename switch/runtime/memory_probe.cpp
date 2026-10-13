#include "horizon_memory.hpp"
#include <cstdio>
#include <cstring>
#include <switch.h>
using fhnx::HorizonMemory;
// Bound libnx's process heap rather than taking its full default allowance.
extern "C" { u32 __nx_heap_size=32*1024*1024; }
static void event(const char* text) { char s[256]; int n=snprintf(s,sizeof(s),"FHNX_VM %s\n",text); svcOutputDebugString(s,n); }
static void gate(bool b,const char* why) {if(!b) throw std::runtime_error(why); event(why);}
template<class F> static bool rejects(F f) {try {f();} catch(const std::exception&) {return true;} return false;}
extern "C" int fhnx_runtime_probe() {
    try {
        HorizonMemory memory(2*4096); memory.reserve(1024*1024);
        gate(memory.reserved()==1024*1024 && memory.committed()==0,"reserve_without_physical_commit_pass");
        HorizonMemory oversized(4096);
        gate(rejects([&]{oversized.reserve(0x120000000ull);}) && oversized.committed()==0,"desktop_4_5_gib_reservation_rejected");
        gate(rejects([&]{memory.load32be(0);}),"uncommitted_access_rejected");
        memory.commit(0); memory.commit(4096);
        gate(memory.load32be(0)==0,"commit_zero_initialized");
        memory.store32be(4094,0x12345678);
        gate(memory.load32be(4094)==0x12345678 && memory.base()[4094]==0x12 && memory.base()[4097]==0x78,"unaligned_cross_page_be_pass");
        gate(rejects([&]{memory.commit(8192);}) && memory.committed()==8192,"budget_oom_rejected");
        gate(rejects([&]{memory.commit(1);}) && rejects([&]{memory.commit(0);}),"alignment_duplicate_rejected");
        gate(!memory.alias(0,8192),"writable_alias_unsupported_rejected");
        Result protection=memory.protect(4096,HorizonMemory::Protection::read);
        MemoryInfo info{}; u32 page=0;
        gate(R_SUCCEEDED(svcQueryMemory(&info,&page,reinterpret_cast<u64>(memory.base()+4096))),"kernel_query_pass");
        if(R_SUCCEEDED(protection)) {
            gate(info.perm==Perm_R,"kernel_read_only_verified");
            gate(rejects([&]{memory.store32be(4094,0xffffffff);}) && memory.load32be(4094)==0x12345678,"cross_page_write_atomic_rejection");
            gate(R_SUCCEEDED(memory.protect(4096,HorizonMemory::Protection::none)),"kernel_no_access_set");
            gate(rejects([&]{memory.load32be(4094);}),"no_access_rejected");
            gate(R_SUCCEEDED(memory.protect(4096,HorizonMemory::Protection::read_write)),"kernel_write_restored");
        } else {
            char message[160];snprintf(message,sizeof(message),"protection_unsupported rc=0x%08x kernel_perm=%u",protection,info.perm);event(message);
            gate(info.perm==Perm_Rw && memory.load32be(4094)==0x12345678,"failed_protection_preserves_state");
            // No software fiction of a protected raw-pointer mapping.
            gate(R_FAILED(memory.protect(4096,HorizonMemory::Protection::none)),"no_access_request_rejected");
        }
        memory.decommit(4096);
        gate(rejects([&]{memory.load32be(4094);}),"decommitted_access_rejected");
        gate(R_SUCCEEDED(svcQueryMemory(&info,&page,reinterpret_cast<u64>(memory.base()+4096))) && info.type==MemType_Unmapped,"kernel_unmap_verified");
        memory.commit(4096); gate(memory.load32be(4096)==0,"recommit_zero_pass");
        memory.decommit(0); memory.decommit(4096); gate(memory.committed()==0,"all_pages_reclaimed");
        // A write straddling an uncommitted page must not partially change the first.
        memory.commit(0); memory.store32be(4092,0x11223344);
        gate(rejects([&]{memory.store32be(4094,0xffffffff);}) && memory.load32be(4092)==0x11223344,"cross_page_write_atomic_rejection");
        memory.decommit(0);
        u64 total=0,used=0,stack=0;
        svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);svcGetInfo(&stack,InfoType_StackRegionSize,CUR_PROCESS_HANDLE,0);
        char line[256]; snprintf(line,sizeof(line),"telemetry process_total=%llu process_used=%llu stack_virtual=%llu",(unsigned long long)total,(unsigned long long)used,(unsigned long long)stack);event(line);
        event("all_passed");return 0;
    } catch(const std::exception& e) {event(e.what());event("blocked_or_failed");return 20;}
}
