// Independent synthetic PPC functions: no translated title instructions.
#include <rex/ppc/context.h>
#include "guest_memory.hpp"
#include <switch.h>
#include <array>
#include <cstdio>
#include <vector>

static constexpr uint32_t kBase = 0x82000000;
// Volatile inputs prevent the compiler from replacing the SIMD test with constants.
static volatile uint32_t test_lanes[4] = {1, 10, 100, 0xffffffff};
static void event(const char* name) {
    char line[128];
    int length = snprintf(line, sizeof(line), "FHNX_CONTEXT %s\n", name);
    svcOutputDebugString(line, length);
}

REX_FUNC(synthetic_add) {
    ctx.r3.u64 += ctx.r4.u64;
    ctx.cr0.compare(ctx.r3.s64, int64_t{42}, ctx.xer);
    // Bounded adapter, no assumption that base is a complete 4 GiB arena.
    fhnx::GuestMemory memory(std::span<uint8_t>(base, 1024*1024), kBase);
    memory.store32(ctx.r5.u32, ctx.r3.u32);
}
PPCFuncMapping PPCFuncMappings[] = {{kBase + 0x100, synthetic_add}, {0, nullptr}};
namespace rex::runtime {
PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
    for (const auto& mapping : PPCFuncMappings) {
        if (!mapping.host) break;
        if (mapping.guest == guest_address) return mapping.host;
    }
    return nullptr; // Fail closed; never call a missing function.
}
}

extern "C" int fhnx_runtime_probe(void) {
    static_assert(REX_ARCH_ARM64 && REX_PLATFORM_SWITCH);
    static_assert(alignof(PPCContext) == 64 && sizeof(PPCVRegister) == 16);
    PPCContext ctx{};
    std::vector<uint8_t> arena(1024*1024);
    ctx.r3.u64 = 19; ctx.r4.u64 = 23; ctx.r5.u32 = kBase + 1;
    auto* function = rex::runtime::ResolveIndirectFunction(kBase + 0x100);
    if (!function || rex::runtime::ResolveIndirectFunction(kBase + 0x104)) return 10;
    function(ctx, arena.data());
    if (ctx.r3.u64 != 42 || !ctx.cr0.eq || ctx.cr0.lt || ctx.cr0.gt ||
        arena[1] || arena[2] || arena[3] || arena[4] != 42) return 11;
    event("ppc_dispatch_memory_passed");

    for (unsigned lane = 0; lane < 4; ++lane) ctx.v0.u32[lane] = test_lanes[lane];
    auto value = simde_mm_loadu_si128(reinterpret_cast<const simde__m128i*>(&ctx.v0));
    value = simde_mm_add_epi32(value, simde_mm_set1_epi32(1));
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(&ctx.v1), value);
    if (ctx.v1.u32[0] != 2 || ctx.v1.u32[1] != 11 || ctx.v1.u32[2] != 101 || ctx.v1.u32[3]) return 12;
    event("simd_lanes_wrap_passed");

    const auto original = rex::platform::FPSCRPlatform::getcsr();
    ctx.fpscr.InitHost();
    for (uint32_t mode = 0; mode < 4; ++mode) {
        ctx.fpscr.storeFromGuest(mode);
        if (ctx.fpscr.loadFromHost() != mode) {
            rex::platform::FPSCRPlatform::setcsr(original); return 13;
        }
    }
    ctx.r14.u64 = 0x12345678; ctx.f14.f64 = 1.5; ctx.v14.u32[2] = 0x87654321;
    ctx.cr2.set_raw(0xa);
    std::array<uint8_t, PPCContext::kNonVolatileSaveSize> saved{};
    ctx.SaveNonVolatiles(saved.data());
    ctx.r14.u64 = 0; ctx.f14.f64 = 0; ctx.v14.u32[2] = 0; ctx.cr2.set_raw(0);
    ctx.RestoreNonVolatiles(saved.data());
    bool ok = ctx.r14.u64 == 0x12345678 && ctx.f14.f64 == 1.5 &&
              ctx.v14.u32[2] == 0x87654321 && ctx.cr2.raw() == 0xa;
    rex::platform::FPSCRPlatform::setcsr(original);
    if (!ok) return 14;
    event("fpcr_nonvolatile_passed");
    event("all_passed");
    return 0;
}
