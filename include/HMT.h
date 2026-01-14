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

struct HMT_entry_t{
    public:
    GETSET(HMT_flag_t, flags)
    GETSET(PimObjId, memid)
    GETSET(size_t, max_len)
    GETSET(size_t, base_addr)
    HMT_entry_t(): flags(HMT_flag_t::NA), max_len(-1){}
    HMT_entry_t(size_t base_addr, size_t max_len, PimObjId memid, HMT_flag_t flags): \
        flags(flags), max_len(max_len), memid(memid), base_addr(base_addr){}


    private:
    HMT_flag_t flags;
    size_t max_len;
    size_t base_addr;
    PimObjId memid;

};

class HMT_table_t{
    std::map<size_t, std::unique_ptr<HMT_entry_t>> hmt_table;

    public:
    // HMT_table_t();
    bool accept_fatptr(size_t varidx, size_t offset, HMT_flag_t flag);
    PimObjId get_pim_obj_id(size_t varidx);
    size_t get_base_addr(size_t varidx);
    void add_new_ent(size_t base_addr, size_t varidx, size_t max_len,\
            PimObjId mem_id, HMT_flag_t access_flag);
};

struct SimdFatptr {
    uint32_t varidx;
    int32_t offset;
};

class SimdMemory {
public:
    void add_region(uint32_t varidx, size_t size_bytes);
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
