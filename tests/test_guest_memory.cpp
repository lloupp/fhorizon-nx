#include "guest_memory.hpp"
#include <array>
#include <stdexcept>
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error("CHECK failed: " #expr); } while (false)
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
  std::array<std::uint8_t, 32> raw{};
  fhnx::GuestMemory mem(raw, 0x82000000u);
  mem.store16(0x82000000u, 0x1234u);
  CHECK(raw[0] == 0x12u && raw[1] == 0x34u);
  CHECK(mem.load16(0x82000000u) == 0x1234u);
  mem.store32(0x82000002u, 0x89ABCDEFu);
  CHECK(raw[2] == 0x89u && raw[5] == 0xEFu);
  CHECK(mem.load32(0x82000002u) == 0x89ABCDEFu);
  mem.store64(0x82000008u, UINT64_C(0x0123456789ABCDEF));
  CHECK(raw[8] == 0x01u && raw[15] == 0xEFu);
  CHECK(mem.load64(0x82000008u) == UINT64_C(0x0123456789ABCDEF));
  bool rejected = false;
  try { (void)mem.load64(0x8200001Du); }
  catch (const std::out_of_range&) { rejected = true; }
  CHECK(rejected);
  rejected = false;
  try { mem.store32(0xFFFFFFFFu, 1u); }
  catch (const std::out_of_range&) { rejected = true; }
  CHECK(rejected);
  rejected = false;
  try { (void)mem.load32(0x81000000u); }
  catch (const std::out_of_range&) { rejected = true; }
  CHECK(rejected);
  std::cout << "PASS: big-endian guest memory + bounds, synthetic data\n";
}
