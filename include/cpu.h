#ifndef __CPU__H__
#define __CPU__H__

#include "pesim-configs.h""
#include <array>
#include <queue>
#include <vector>
#include <queue>
#include "HMT.h"

#define ROUND_UP(x, y) \
    (((x) + (y) - 1) / (y))

class SimdCpu {
public:
    explicit SimdCpu(SimdMemory *memory);

    void load_program(const std::vector<SimdInstruction> &program);
    void load_trace(const std::priority_queue<trace_ent_t> &trace);
    void tick();
    void run(size_t max_cycles);
    void inc_cycl();
    bool is_stopped() const;
    bool ready4signal();

    const std::array<uint32_t, 4> &get_vreg(size_t idx) const;
    void set_vreg(size_t idx, const std::array<uint32_t, 4> &value);

    SimdFatptr get_freg(size_t idx) const;
    void set_freg(size_t idx, const SimdFatptr &value);
    void pause();
    void resume();

    void reset();

    void set_timing(int ccd, int rp, int rcd){
        tCCD_L = ccd; tRP = rp; tRCD = rcd;
    }

private:
    static constexpr size_t kVectorRegisters = 6;
    static constexpr size_t kFatptrRegisters = 4;
    static constexpr size_t kVectorBytes = 16;

    //Parameter comes from DDR4_4Gb_x16_3200
    int tCCD_L = 8;
    int tRP = 22;
    int tRCD = 22;

    //should be the same as in HMT.h
    int tRP_FAST = 12; 
    int tRCD_FAST = 12;

#ifndef MASA_TLDRAM
    int pre_pause_hold_cycl = ROUND_UP(std::max(tRP + tRCD - tCCD_L, tCCD_L), tCCD_L) - 1;
    int post_resume_hold_cycl = ROUND_UP(tRP + tRCD, tCCD_L) - 1;
#else
    int pre_pause_hold_cycl = ROUND_UP(std::max(tRP_FAST + tRCD_FAST - tCCD_L, tCCD_L), tCCD_L) - 1;
    int post_resume_hold_cycl = ROUND_UP(tRP_FAST + tRCD_FAST, tCCD_L) - 1;
#endif
    

    struct IfId {
        bool valid{false};
        size_t pc{0};
        SimdInstruction inst{};
        bool pend_cpu_stop{false};
    };

    struct IdHmt {
        bool valid{false};
        SimdInstruction inst{};
        bool pend_cpu_stop{false};
    };

    struct HmtEx {
        bool valid{false};
        SimdInstruction inst{};
        SimdFatptr fatptr{0, 0};
        size_t phys_addr{0};
    };

    struct ExMem {
        bool valid{false};
        SimdInstruction inst{};
        SimdFatptr fatptr{0, 0};
        size_t phys_addr{0};
        SimdFatptr fatptr_result{0, 0};
        std::array<uint32_t, 4> vec_result{};
        std::array<uint32_t, 4> vec_operand{};
        size_t mem_delay_remaining{0};
    };

    struct MemWb {
        bool valid{false};
        SimdInstruction inst{};
        SimdFatptr fatptr_result{0, 0};
        std::array<uint32_t, 4> vec_result{};
        bool should_stop{false};
    };

    bool uses_fatptr(const SimdInstruction &inst) const;
    void validate_reg_index(int idx, size_t max, const char *err_msg) const;

    SimdMemory *memory_;
    std::vector<SimdInstruction> program_;
    std::priority_queue<trace_ent_t> traces_;

    size_t pc_{0};
    size_t cycl{0};

    int hold_cntr{0};
    bool cpu_stop_{false};
    bool cpu_pause_{false};
    bool cpu_post_resume_delay{false};
    bool cpu_ready4signal{true};

    std::array<std::array<uint32_t, 4>, kVectorRegisters> vregs_{};
    std::array<SimdFatptr, kFatptrRegisters> fregs_{};

    IfId if_id_{};
    IdHmt id_hmt_{};
    HmtEx hmt_ex_{};
    ExMem ex_mem_{};
    MemWb mem_wb_{};
};

#endif
