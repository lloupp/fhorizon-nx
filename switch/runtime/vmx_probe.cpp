// Actual pinned ShiftGlue SIMD helpers on ARM64, independent runtime inputs.
#include <rex/ppc/context.h>
#include <rex/ppc/intrinsics.h>
#include <switch.h>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" { u32 __nx_heap_size=32*1024*1024; }
static volatile uint8_t byte_a[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
static volatile uint8_t byte_b[16]={16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
static volatile uint8_t control[16]={0,31,1,16,5,22,15,17,8,24,3,19,255,32,47,63};
static volatile uint8_t sat_a[16]={250,0,127,128,1,254,100,200,255,42,30,40,50,60,70,80};
static volatile uint8_t sat_b[16]={10,255,130,128,0,2,100,100,1,42,230,220,210,200,190,180};
static volatile int16_t signed_a[8]={32767,-32768,100,-100,30000,-30000,0,-1};
static volatile int16_t signed_b[8]={1,-1,200,-200,10000,-10000,-1,1};
static volatile float conversions[4]={1.75f,-2.75f,2147483648.0f,-2147483904.0f};
static volatile uint32_t unsigned_lanes[4]={0,1,2147483648u,4294967295u};
static volatile float unsigned_floats[4]={-1,42.75f,4294967296.0f,std::numeric_limits<float>::quiet_NaN()};
static volatile float dot_a[4]={2,3,4,5};
static volatile float dot_b[4]={7,11,13,17};

static void event(const char* text) {char s[160];int n=snprintf(s,sizeof(s),"FHNX_VMX %s\n",text);svcOutputDebugString(s,n);}
template<class T,size_t N> static void runtime_copy(T (&out)[N],volatile T (&in)[N]) {for(size_t i=0;i<N;++i)out[i]=in[i];}
// Named, non-inlined entry points permit inspection of executed ARM64 code.
extern "C" __attribute__((noinline)) simde__m128i vmx_permute(simde__m128i a,simde__m128i b,simde__m128i c) {return rex::ppc::simde_mm_perm_epi8_(a,b,c);}
extern "C" __attribute__((noinline)) simde__m128i vmx_sat_unsigned(simde__m128i a,simde__m128i b) {return simde_mm_adds_epu8(a,b);}
extern "C" __attribute__((noinline)) simde__m128i vmx_sat_signed(simde__m128i a,simde__m128i b) {return simde_mm_adds_epi16(a,b);}
extern "C" __attribute__((noinline)) simde__m128i vmx_compare(simde__m128i a,simde__m128i b) {return rex::ppc::simde_mm_cmpgt_epu8(a,b);}
extern "C" __attribute__((noinline)) simde__m128i vmx_convert_signed(simde__m128 a) {return rex::ppc::simde_mm_vctsxs(a);}
extern "C" __attribute__((noinline)) simde__m128i vmx_convert_unsigned(simde__m128 a) {return rex::ppc::simde_mm_vctuxs(a);}
extern "C" __attribute__((noinline)) simde__m128 vmx_uint_to_float(simde__m128i a) {return rex::ppc::simde_mm_cvtepu32_ps_(a);}
extern "C" __attribute__((noinline)) simde__m128 vmx_dot3(simde__m128 a,simde__m128 b) {return rex::ppc::simde_mm_vmsum3fp128(a,b);}
extern "C" __attribute__((noinline)) simde__m128 vmx_dot4(simde__m128 a,simde__m128 b) {return rex::ppc::simde_mm_vmsum4fp128(a,b);}
extern "C" __attribute__((noinline)) void vmx_copy_unaligned(const uint8_t* source,uint8_t* dest) {
    auto value=simde_mm_loadu_si128(reinterpret_cast<const simde__m128i*>(source));
    // Keep this alignment gate in a SIMD register; otherwise GCC legitimately
    // turns an identity vector copy into paired scalar loads/stores.
    asm volatile("" : "+w"(value) : : "memory");
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(dest),value);
}
extern "C" __attribute__((noinline)) simde__m128i vmx_endian_shuffle(simde__m128i a,simde__m128i mask) {return simde_mm_shuffle_epi8(a,mask);}

extern "C" int fhnx_runtime_probe() {
    static_assert(REX_ARCH_ARM64 && REX_PLATFORM_SWITCH && alignof(PPCContext)==64);
    uint8_t a[16],b[16],c[16],out[16];runtime_copy(a,byte_a);runtime_copy(b,byte_b);runtime_copy(c,control);
    auto load=[](const void* p){return simde_mm_loadu_si128(static_cast<const simde__m128i*>(p));};
    auto store=[](void* p,simde__m128i v){simde_mm_storeu_si128(static_cast<simde__m128i*>(p),v);};
    store(out,vmx_permute(load(a),load(b),load(c)));
    // PPC guest byte 0 corresponds to host byte 15. Controls use low five bits.
    const uint8_t perm_reference[16]={15,16,14,31,10,25,0,30,7,23,12,28,16,15,0,16};
    if(std::memcmp(out,perm_reference,16))return 31;
    event("permute_guest_byte_order_pass");
    runtime_copy(a,sat_a);runtime_copy(b,sat_b);store(out,vmx_sat_unsigned(load(a),load(b)));
    const uint8_t sat_reference[16]={255,255,255,255,1,255,200,255,255,84,255,255,255,255,255,255};
    if(std::memcmp(out,sat_reference,16))return 32;
    event("unsigned_saturation_pass");
    int16_t sa[8],sb[8],so[8];runtime_copy(sa,signed_a);runtime_copy(sb,signed_b);store(so,vmx_sat_signed(load(sa),load(sb)));
    const int16_t signed_reference[8]={32767,-32768,300,-300,32767,-32768,-1,0};
    if(std::memcmp(so,signed_reference,16))return 33;
    event("signed_saturation_pass");
    store(out,vmx_compare(load(a),load(b)));
    const uint8_t compare_reference[16]={255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0};
    if(std::memcmp(out,compare_reference,16))return 34;
    event("unsigned_compare_masks_pass");
    // Real unaligned load/store at every displacement, plus four-byte BE reversal.
    alignas(16) uint8_t unaligned[48]{};
    for(unsigned offset=1;offset<16;++offset) {
        std::memcpy(unaligned+offset,a,16);vmx_copy_unaligned(unaligned+offset,out);
        if(std::memcmp(a,out,16))return 35;
        vmx_copy_unaligned(b,unaligned+offset);if(std::memcmp(b,unaligned+offset,16))return 36;
    }
    runtime_copy(a,byte_a);
    const uint8_t reverse[16]={3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12};
    store(out,vmx_endian_shuffle(load(a),load(reverse)));
    if(std::memcmp(out,reverse,16))return 37;
    event("unaligned_and_endian_shuffle_pass");
    const auto original=rex::platform::FPSCRPlatform::getcsr();
    PPCContext ctx{};ctx.fpscr.InitHost();ctx.fpscr.storeFromGuest(0);
    float f[4];runtime_copy(f,conversions);int32_t ints[4];store(ints,vmx_convert_signed(simde_mm_loadu_ps(f)));
    const int32_t signed_conversion[4]={1,-2,INT32_MAX,INT32_MIN};
    if(std::memcmp(ints,signed_conversion,16)) {rex::platform::FPSCRPlatform::setcsr(original);return 38;}
    runtime_copy(f,unsigned_floats);uint32_t uints[4];store(uints,vmx_convert_unsigned(simde_mm_loadu_ps(f)));
    const uint32_t unsigned_conversion[4]={0,42,UINT32_MAX,0};
    if(std::memcmp(uints,unsigned_conversion,16)) {rex::platform::FPSCRPlatform::setcsr(original);return 39;}
    runtime_copy(uints,unsigned_lanes);simde_mm_storeu_ps(f,vmx_uint_to_float(load(uints)));
    const uint32_t float_reference[4]={0x00000000,0x3f800000,0x4f000000,0x4f800000};
    if(std::memcmp(f,float_reference,16)) {rex::platform::FPSCRPlatform::setcsr(original);return 40;}
    event("saturating_conversions_nan_pass");
    float da[4],db[4];runtime_copy(da,dot_a);runtime_copy(db,dot_b);
    simde_mm_storeu_ps(f,vmx_dot3(simde_mm_loadu_ps(da),simde_mm_loadu_ps(db)));
    for(auto v:f)if(v!=170.0f){rex::platform::FPSCRPlatform::setcsr(original);return 41;}
    simde_mm_storeu_ps(f,vmx_dot4(simde_mm_loadu_ps(da),simde_mm_loadu_ps(db)));
    for(auto v:f)if(v!=184.0f){rex::platform::FPSCRPlatform::setcsr(original);return 42;}
    event("guest_order_float_dot_pass");
    // Ensure adding VMX work retains the local FPCR overlay and context ABI.
    for(unsigned mode=0;mode<4;++mode){ctx.fpscr.storeFromGuest(mode);if(ctx.fpscr.loadFromHost()!=mode){rex::platform::FPSCRPlatform::setcsr(original);return 43;}}
    ctx.v14.u64[0]=0x123456789abcdef0ull;ctx.v31.u64[1]=0xfedcba9876543210ull;
    ctx.r14.u64=123;ctx.f14.f64=2.5;
    std::array<uint8_t,PPCContext::kNonVolatileSaveSize> save{};ctx.SaveNonVolatiles(save.data());
    ctx.v14.u64[0]=ctx.v31.u64[1]=ctx.r14.u64=0;ctx.f14.f64=0;ctx.RestoreNonVolatiles(save.data());
    rex::platform::FPSCRPlatform::setcsr(original);
    if(ctx.v14.u64[0]!=0x123456789abcdef0ull || ctx.v31.u64[1]!=0xfedcba9876543210ull || ctx.r14.u64!=123 || ctx.f14.f64!=2.5)return 44;
    event("fpcr_vector_context_preserved_pass");event("all_passed");return 0;
}
