#ifndef __SIM_H__
#define __SIM_H__

#include "cpu.h"
#include "HMT.h"
#include <vector>

class SimdSim {
    SimdMemory memory_;
    SimdCpu cpu_;
    std::vector<SimdInstruction> program_;

public:
    SimdSim();
    void run(size_t max_cycles);
    SimdCpu &cpu();
    const SimdCpu &cpu() const;
};

#endif
