#ifndef __SIM_H__
#define __SIM_H__

#include "configs.h"
#include "cpu.h"
#include "HMT.h"
#include "memory_system.h"
#include <vector>
#include <queue>
// #include "../dramsim3/DRAMSim3/src/memory_system.h"


class SimdSim {
    SimdMemory memory_;
    SimdCpu cpu_;
    std::vector<SimdInstruction> program_;
    std::priority_queue<trace_ent_t> traces_;
    // dramsim3::MemorySystem *dramsim3;

public:
    SimdSim();
    void add_region(uint32_t varidx, size_t size_bytes);
    void fill_region(uint32_t varidx, const std::vector<uint8_t> &src);
    void set_vreg(size_t idx, const std::array<uint32_t, 4> &value);
    void load_program(const std::vector<SimdInstruction> &program);
    bool empty_program();
    void load_trace(const std::priority_queue<trace_ent_t> &traces_);
    void run(size_t max_cycles);
    SimdCpu &cpu();
    const SimdCpu &cpu() const;
    SimdMemory &memory();
    const SimdMemory &memory() const;
};

#endif
