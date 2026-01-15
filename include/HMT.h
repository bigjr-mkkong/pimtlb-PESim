#ifndef __HMT_H__
#define __HMT_H__

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include "libpimeval.h"

#define GETSET(T, N) \
  T get_##N() const { return N; };  \
  void set_##N(T x) { N = x; }

enum HMT_flag_t{
    R,
    W,
    RW,
    NA,
};

struct SimdFatptr {
    uint32_t varidx;
    int32_t offset;
};

class SimdMemory {
public:
    void add_region(uint32_t varidx, size_t size_bytes);
    void fill_region(uint32_t varidx, const std::vector<uint8_t> &data);
    bool check_fatptr(const SimdFatptr &ptr, size_t size_bytes) const;
    size_t translate_fatptr(const SimdFatptr &ptr, size_t size_bytes) const;
    std::array<uint32_t, 4> load128(size_t phys_addr) const;
    void store128(size_t phys_addr, const std::array<uint32_t, 4> &value);
    bool equal128(size_t phys_addr, const std::array<uint32_t, 4> &value) const;
    size_t get_delay_cycl(size_t phys_addr) const;

private:
    struct Region {
        size_t base_addr;
        std::vector<uint8_t> data;
    };
    const Region *find_region(size_t phys_addr) const;
    Region *find_region(size_t phys_addr);
    std::unordered_map<uint32_t, Region> regions_;
};

#endif
