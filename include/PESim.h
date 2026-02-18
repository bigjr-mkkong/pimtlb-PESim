#ifndef __SIM_H__
#define __SIM_H__

#include "configs.h"
#include "cpu.h"
#include "HMT.h"
#include <vector>
#include <queue>
#include "ext_sig.h"


class SimdSim {
    SimdMemory memory_;
    SimdCpu cpu_;
    std::vector<SimdInstruction> program_;
    std::priority_queue<ext_sig_t> trace_;

public:
    SimdSim();
    void add_region(uint32_t varidx, size_t size_bytes);
    void load_program(const std::vector<SimdInstruction> &program);
    bool empty_program();
    void load_trace(const std::priority_queue<ext_sig_t> &trace);
    void run(size_t max_cycles);
    SimdCpu &cpu();
    const SimdCpu &cpu() const;
    SimdMemory &memory();
    const SimdMemory &memory() const;
};

#endif
