#pragma once
#include <switch.h>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace fhnx {
// Sparse Horizon stack-region mappings. svcMapMemory MOVES heap pages: it is
// deliberately never advertised as a simultaneous shared writable alias.
class HorizonMemory {
public:
    enum class Protection { none, read, read_write };
    static constexpr size_t page_size=4096;
private:
    struct Page { void* source; Protection protection; };
    uint8_t* base_{};
    VirtmemReservation* reservation_{};
    size_t size_{}, budget_{};
    std::map<size_t,Page> pages_;
    static void success(Result,const char*);
    void range(size_t,size_t) const;
    void access(size_t,size_t,bool) const;
public:
    explicit HorizonMemory(size_t physical_budget);
    ~HorizonMemory();
    HorizonMemory(const HorizonMemory&)=delete;
    HorizonMemory& operator=(const HorizonMemory&)=delete;
    void reserve(size_t bytes);
    void commit(size_t offset); // Exactly one page, zero initialized.
    void decommit(size_t offset);
    // Returns the real kernel status; failed permission changes leave state intact.
    Result protect(size_t offset,Protection);
    // Genuine writable aliases are not supplied by svcMapMemory; fail closed.
    bool alias(size_t,size_t) const { return false; }
    void store32be(size_t offset,uint32_t);
    uint32_t load32be(size_t offset) const;
    uint8_t* base() const {return base_;}
    size_t reserved() const {return size_;}
    size_t committed() const {return pages_.size()*page_size;}
};
}
