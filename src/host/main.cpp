#include "guest_memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>
int main() {
  std::array<std::uint8_t, 16> guest{};
  fhnx::GuestMemory mem(guest, 0x82000000u);
  mem.store32(0x82000000u, 0x4D5309C9u); // FH1 Title ID as synthetic test constant
  if (mem.load32(0x82000000u) != 0x4D5309C9u) return 1;
  std::cout << "FHNX PC smoke: OK (synthetic guest memory; no game executed)\n";
}
