#include "Sim.h"

SimdSim::SimdSim() : cpu_(&memory_) {
    program_.push_back(SimdInstruction{
        SimdOpcode::Add128,
        0,
        1,
        2,
        -1,
        -1,
        0,
        0,
        SimdFatptr{0, 0},
    });
}

void SimdSim::run(size_t max_cycles) {
    cpu_.load_program(program_);
    cpu_.run(max_cycles);
}

SimdCpu &SimdSim::cpu() {
    return cpu_;
}

const SimdCpu &SimdSim::cpu() const {
    return cpu_;
}
