#ifndef __HMT_H__
#define __HMT_H__

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

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

class tiny_dram_bank{
    public:
        bool is_hit(size_t paddr);
        bool is_first_access();
        void update_last_read(size_t cycl);
        void update_last_write(size_t cycl);
        void update_last_act(size_t cycl);
        int get_prec_delay(size_t cycl);

        void set_timing(int ras, int rtp, int wr){
            tRAS = ras; tRTP = rtp; tWR = wr;
        }
    private:
    const size_t rows = 65536;
    const size_t columns = 1024;
    const size_t BL = 8;
    const size_t sz_per_row = columns * BL;

    int tRAS = 52; //minimun time between act and prec
    int tRTP = 12; //minimum time between read and prec
    int tWR = 24; //minimum time between write and prec

    long long last_opened_row{-1};
    size_t t_last_read{0};
    size_t t_last_write{0};
    size_t t_last_act{0};
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
    size_t get_delay_cycl(size_t phys_addr, bool is_read, size_t cur_cycl);
    tiny_dram_bank &bank_model();

private:
    struct Region {
        size_t base_addr;
        std::vector<uint8_t> data;
    };
    const Region *find_region(size_t phys_addr) const;
    Region *find_region(size_t phys_addr);
    std::unordered_map<uint32_t, Region> regions_;

    tiny_dram_bank dram_bank;
};

#endif
