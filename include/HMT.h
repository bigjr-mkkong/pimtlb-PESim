#ifndef __HMT_H__
#define __HMT_H__

#include <array>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

#include "../../src/pimPerfEnergyBase.h""

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
        void update_hit(size_t paddr);
        bool is_active(size_t paddr);

        int get_prec_delay(size_t cycl);
        int get_rd_delay(size_t cycl);
        int get_wr_delay(size_t cycl);

        void push_ddr(pimeval::EventNode *ev);

        void set_timing(int ras, int rtp, int wr, int rcdrd,\
                int rp, int ccd_l, int sa_sel, int wtr){
            tRAS = ras; tRTP = rtp; tWR = wr; tRCDRD = rcdrd; tRP = rp; tCCDL = ccd_l;
            tSA_SEL = sa_sel; tWTR = wtr;
        }
        int executeMemoryEvent(size_t currCycle);


    private:
    const size_t SA_num = 32;
    const size_t row_per_SA = 512;
    const size_t columns = 1024;
    const size_t BL = 8;
    const size_t sz_per_row = columns * BL;
    const size_t sz_per_SA = sz_per_row * row_per_SA;

    int tRAS = 52; //minimun time between act and prec
    int tRTP = 12; //minimum time between read and prec
    int tWR = 24; //minimum time between write and prec
    int tWTR = 3; //minimum time of write after read delay
    int tRCDRD = 22;
    int tRP = 22;
    int tCCDL = 8;

    int tSA_SEL = 4;

    // long long last_opened_row{-1};
    size_t t_last_read{0};
    size_t t_last_write{0};
    size_t t_last_act{0};

    int sa_sel_table[256] = {-1};
    std::queue<pimeval::EventNode> ddr_events;
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
