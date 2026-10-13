// Small real ShiftGlue header probe; this does not execute translated game code.
#include <rex/types.h>
#include "guest_memory.hpp"
#include <array>
#include <cstring>
#include <vector>

extern "C" int fhnx_runtime_probe(void) {
    static_assert(sizeof(void*) == 8);
    static_assert(REX_ARCH_ARM64 && REX_PLATFORM_SWITCH);
    if (rex::byte_swap(uint16_t{0x1234}) != 0x3412 ||
        rex::byte_swap(uint32_t{0x12345678}) != 0x78563412 ||
        rex::byte_swap(uint64_t{0x0102030405060708}) != 0x0807060504030201)
        return 1;
    rex::be_u32 upstream_value{0x12345678};
    std::array<uint8_t, 4> encoded{};
    std::memcpy(encoded.data(), &upstream_value, encoded.size());
    if (encoded != std::array<uint8_t, 4>{0x12, 0x34, 0x56, 0x78}) return 2;

    // Bounded synthetic region, NOT ShiftGlue's full virtual/physical arena.
    std::vector<uint8_t> arena(1024 * 1024);
    fhnx::GuestMemory memory(arena, 0x82000000);
    memory.store32(0x82000001, upstream_value.get());
    memory.store64(0x820ffff8, 0x0102030405060708);
    if (memory.load32(0x82000001) != upstream_value.get() ||
        memory.load64(0x820ffff8) != 0x0102030405060708) return 3;
    try { memory.load64(0x820ffff9); return 4; }
    catch (const std::out_of_range&) {}
    return 0;
}
