#ifndef __CPU__H__
#define __CPU__H__

#include <array>
#include <vector>
#include "HMT.h"

enum class SimdOpcode {
    Add128,
    Ld128,
    St128,
    Jump,
    EqualExit,
    FatptrLi,
    FatptrAdd,
    FatptrSub,
    Nop,
};

struct SimdInstruction {
    SimdOpcode opcode{SimdOpcode::Nop};
    int rd{-1};
    int rs1{-1};
    int rs2{-1};
    int frd{-1};
    int frs1{-1};
    int imm{0};
    int mask{0};
    SimdFatptr fatptr_imm{0, 0};
};

class SimdCpu {
public:
    explicit SimdCpu(SimdMemory *memory);

    void load_program(const std::vector<SimdInstruction> &program);
    void tick();
    void run(size_t max_cycles);
    bool is_stopped() const;

    const std::array<uint32_t, 4> &get_vreg(size_t idx) const;
    void set_vreg(size_t idx, const std::array<uint32_t, 4> &value);

    SimdFatptr get_freg(size_t idx) const;
    void set_freg(size_t idx, const SimdFatptr &value);

private:
    static constexpr size_t kVectorRegisters = 6;
    static constexpr size_t kFatptrRegisters = 4;
    static constexpr size_t kVectorBytes = 16;

    struct IfId {
        bool valid{false};
        size_t pc{0};
        SimdInstruction inst{};
    };

    struct IdHmt {
        bool valid{false};
        SimdInstruction inst{};
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
        bool jump_taken{false};
        size_t jump_target{0};
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
    void validate_reg_index(int idx, size_t max, const char *name) const;

    SimdMemory *memory_;
    std::vector<SimdInstruction> program_;

    size_t pc_{0};
    bool cpu_stop_{false};

    std::array<std::array<uint32_t, 4>, kVectorRegisters> vregs_{};
    std::array<SimdFatptr, kFatptrRegisters> fregs_{};

    IfId if_id_{};
    IdHmt id_hmt_{};
    HmtEx hmt_ex_{};
    ExMem ex_mem_{};
    MemWb mem_wb_{};
};

#endif
