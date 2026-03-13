#ifndef __SIM_H__
#define __SIM_H__

#include "configs.h"
#include "cpu.h"
#include "HMT.h"
#include "memory_system.h"
#include <algorithm>
#include <vector>
#include <queue>
#include <map>
#include <memory>


class SimdSim {
    SimdMemory memory_;
    SimdCpu cpu_;
    std::vector<SimdInstruction> program_;
    std::priority_queue<trace_ent_t> traces_;

    std::unique_ptr<dramsim3::MemorySystem> dramsim3;
    std::map<uint64_t, int> pendmap;

    void dramsim3_read_callback(uint64_t addr);
    void dramsim3_write_callback(uint64_t addr);
    bool dramsim3_empty();
    void run_MEM(sim_option_t opt);
    void run_PIM(sim_option_t opt);
    void run_HYBRID(sim_option_t opt);

public:
    SimdSim();
    void add_region(uint32_t varidx, size_t size_bytes);
    void fill_region(uint32_t varidx, const std::vector<uint8_t> &src);
    void set_vreg(size_t idx, const std::array<uint32_t, 4> &value);
    void load_program(const std::vector<SimdInstruction> &program);
    bool empty_program();
    void load_trace(const std::priority_queue<trace_ent_t> &traces_);
    void run(sim_option_t opt);
    SimdCpu &cpu();
    const SimdCpu &cpu() const;
    SimdMemory &memory();
    const SimdMemory &memory() const;


};

#endif
