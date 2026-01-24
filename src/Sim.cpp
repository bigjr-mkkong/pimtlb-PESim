#include "PESim.h"

SimdSim::SimdSim() : cpu_(&memory_) {
    for(int i=0; i<100; i++)
    program_.push_back(SimdInstruction{
        SimdOpcode::Add128,
        1,
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


void SimdSim::add_region(uint32_t varidx, size_t size_bytes) {
    memory_.add_region(varidx, size_bytes);
}

void SimdSim::load_program(const std::vector<SimdInstruction> &program) {
    program_ = program;
    cpu_.load_program(program_);
}
