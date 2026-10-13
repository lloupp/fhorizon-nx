#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
extern "C" {u32 __nx_heap_size=32*1024*1024;}
static void event(const char* name,u64 value){char s[256];int n=snprintf(s,sizeof(s),"FHNX_LAYOUT %s=%llu hex=0x%llx\n",name,(unsigned long long)value,(unsigned long long)value);svcOutputDebugString(s,n);}
static void gate(bool ok,const char* name){if(!ok)throw std::runtime_error(name);event(name,1);}
static u64 info(u32 kind){u64 value=0;Result rc=svcGetInfo(&value,kind,CUR_PROCESS_HANDLE,0);if(R_FAILED(rc))throw std::runtime_error("GetInfo failed");return value;}
// Single-threaded probe reservation within a queried region, without committing
// its entire range. The lock prevents collision with cooperating libnx allocators.
struct Reservation{
 void* address{};VirtmemReservation* token{};size_t size{};
 Reservation(u64 start,u64 span,size_t bytes):size(bytes){
  if(bytes>span)return;
  virtmemLock();u64 p=start;
  for(unsigned n=0;n<64 && p<=start+span-bytes;++n){MemoryInfo m{};u32 page=0;if(R_FAILED(svcQueryMemory(&m,&page,p)))break;
   if(m.type==MemType_Unmapped && m.addr<=p && m.size>=bytes && p-m.addr<=m.size-bytes){address=reinterpret_cast<void*>(p);token=virtmemAddReservation(address,bytes);break;}
   if(m.addr+m.size<=p)break;
   p=m.addr+m.size;
  }
  virtmemUnlock();
 }
 ~Reservation(){if(token){virtmemLock();virtmemRemoveReservation(token);virtmemUnlock();}}
};
extern "C" int fhnx_runtime_probe(){try{
 u64 aslr=info(InfoType_AslrRegionAddress),aslr_size=info(InfoType_AslrRegionSize),alias=info(InfoType_AliasRegionAddress),alias_size=info(InfoType_AliasRegionSize);
 event("aslr_address",aslr);event("aslr_size",aslr_size);event("alias_address",alias);event("alias_size",alias_size);event("stack_address",info(InfoType_StackRegionAddress));event("stack_size",info(InfoType_StackRegionSize));event("heap_size",info(InfoType_HeapRegionSize));
 {Reservation large(alias,alias_size,0x120000000ull);event("alias_4_5_gib_bookkeeping_reservation",large.token!=nullptr);}
 event("map_physical_hinted",envIsSyscallHinted(0x2c));event("create_shared_hinted",envIsSyscallHinted(0x50));
 {Reservation one(alias,alias_size,4096);
  if(one.token && envIsSyscallHinted(0x2c)){
   Result rc=svcMapPhysicalMemory(one.address,4096);event("map_physical_rc",rc);
   if(R_SUCCEEDED(rc)){
    auto* p=static_cast<volatile u32*>(one.address);p[0]=0x12345678;gate(p[0]==0x12345678,"physical_rw_pass");
    Result protect=svcSetMemoryPermission(one.address,4096,Perm_R);event("physical_protect_read_rc",protect);
    MemoryInfo m{};u32 page=0;gate(R_SUCCEEDED(svcQueryMemory(&m,&page,reinterpret_cast<u64>(one.address))),"physical_query_pass");event("physical_kernel_permission",m.perm);
    if(R_SUCCEEDED(protect))gate(m.perm==Perm_R,"physical_read_only_kernel_pass");
    event("physical_restore_rw_rc",svcSetMemoryPermission(one.address,4096,Perm_Rw));
    gate(R_SUCCEEDED(svcUnmapPhysicalMemory(one.address,4096)),"physical_unmap_pass");
   }
  }
 }
 // Measure MOVE semantics independently, including source permission and restoration.
 void* source=std::aligned_alloc(4096,4096);gate(source!=nullptr,"move_source_allocated");static_cast<u32*>(source)[0]=0xaabbccdd;
 Result heap_read=svcSetMemoryPermission(source,4096,Perm_R);event("heap_protect_read_rc",heap_read);
 if(R_SUCCEEDED(heap_read)){
  MemoryInfo m{};u32 page=0;gate(R_SUCCEEDED(svcQueryMemory(&m,&page,reinterpret_cast<u64>(source))) && m.perm==Perm_R,"heap_read_only_kernel_pass");
  gate(R_SUCCEEDED(svcSetMemoryPermission(source,4096,Perm_Rw)),"heap_write_permission_restored");
 }
 event("map_process_hinted",envIsSyscallHinted(0x74));
 if(envIsSyscallHinted(0x74)){
  virtmemLock();void* other=virtmemFindAslr(4096,4096);auto* reservation=other?virtmemAddReservation(other,4096):nullptr;virtmemUnlock();
  gate(reservation!=nullptr,"process_alias_destination_reserved");
  Result mapped=svcMapProcessMemory(other,CUR_PROCESS_HANDLE,reinterpret_cast<u64>(source),4096);event("self_map_process_rc",mapped);
  if(R_SUCCEEDED(mapped)){
   MemoryInfo a{},b{};u32 page=0;gate(R_SUCCEEDED(svcQueryMemory(&a,&page,reinterpret_cast<u64>(source))) && R_SUCCEEDED(svcQueryMemory(&b,&page,reinterpret_cast<u64>(other))),"self_map_queries_pass");
   event("self_map_source_permission",a.perm);event("self_map_dest_permission",b.perm);
   if(a.perm==Perm_Rw && b.perm==Perm_Rw){auto* pa=static_cast<volatile u32*>(source);auto* pb=static_cast<volatile u32*>(other);pa[0]=42;gate(pb[0]==42,"self_alias_forward_pass");pb[0]=0xaabbccdd;gate(pa[0]==0xaabbccdd,"self_alias_reverse_pass");}
   gate(R_SUCCEEDED(svcUnmapProcessMemory(other,CUR_PROCESS_HANDLE,reinterpret_cast<u64>(source),4096)),"self_alias_unmap_pass");
  }
  virtmemLock();virtmemRemoveReservation(reservation);virtmemUnlock();
 }
 virtmemLock();void* dest=virtmemFindStack(4096,4096);auto* token=dest?virtmemAddReservation(dest,4096):nullptr;virtmemUnlock();
 gate(token!=nullptr,"move_destination_reserved");Result move=svcMapMemory(dest,source,4096);event("move_map_rc",move);
 if(R_SUCCEEDED(move)){
  MemoryInfo src{},dst{};u32 page=0;gate(R_SUCCEEDED(svcQueryMemory(&src,&page,reinterpret_cast<u64>(source))) && R_SUCCEEDED(svcQueryMemory(&dst,&page,reinterpret_cast<u64>(dest))),"move_queries_pass");
  gate(src.perm==Perm_None && dst.perm==Perm_Rw && static_cast<u32*>(dest)[0]==0xaabbccdd,"move_is_not_simultaneous_alias_pass");
  gate(R_SUCCEEDED(svcUnmapMemory(dest,source,4096)),"move_unmap_pass");gate(static_cast<u32*>(source)[0]==0xaabbccdd,"move_source_restored_pass");
 }
 std::free(source);virtmemLock();virtmemRemoveReservation(token);virtmemUnlock();
 if(envIsSyscallHinted(0x50)){
  Handle shared=0;Result rc=svcCreateSharedMemory(&shared,4096,Perm_Rw,Perm_Rw);event("create_shared_rc",rc);
  if(R_SUCCEEDED(rc)){
   // Two mappings of a genuine shared handle; no duplicated backing buffer.
   virtmemLock();void* a=virtmemFindAslr(4096,4096);auto* ra=a?virtmemAddReservation(a,4096):nullptr;void* b=virtmemFindAslr(4096,4096);auto* rb=b?virtmemAddReservation(b,4096):nullptr;virtmemUnlock();
   gate(ra && rb,"shared_virtual_reservations");Result ma=svcMapSharedMemory(shared,a,4096,Perm_Rw);event("shared_first_map_rc",ma);Result mb=svcMapSharedMemory(shared,b,4096,Perm_Rw);event("shared_second_map_rc",mb);
   if(R_SUCCEEDED(ma) && R_SUCCEEDED(mb)){auto* pa=static_cast<volatile u32*>(a);auto* pb=static_cast<volatile u32*>(b);pa[0]=42;gate(pb[0]==42,"shared_forward_coherence_pass");pb[0]=99;gate(pa[0]==99,"shared_reverse_coherence_pass");}
   if(R_SUCCEEDED(ma))gate(R_SUCCEEDED(svcUnmapSharedMemory(shared,a,4096)),"shared_first_unmap_pass");
   if(R_SUCCEEDED(mb))gate(R_SUCCEEDED(svcUnmapSharedMemory(shared,b,4096)),"shared_second_unmap_pass");
   virtmemLock();virtmemRemoveReservation(ra);virtmemRemoveReservation(rb);virtmemUnlock();svcCloseHandle(shared);
  }
 }else event("shared_creation_unavailable_no_call",1);
 event("process_used",info(InfoType_UsedMemorySize));event("all_measured",1);return 0;
 }catch(const std::exception& e){char s[256];int n=snprintf(s,sizeof(s),"FHNX_LAYOUT FAILED %s\n",e.what());svcOutputDebugString(s,n);return 31;}}
