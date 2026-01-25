#include "Sim.h"

SimdSim::SimdSim() : cpu_(&memory_) {
    program_.push_back(SimdInstruction{
        .opcode = SimdOpcode::Add128,
        .rd = 1,
        .rs1 = 1,
        .rs2 = 2,
        .imm = 0,
        .mask = 0,
        .fatptr_imm = {0, 0},
    });
    program_.push_back(SimdInstruction{
        .opcode = SimdOpcode::Jump,
        .rd = 0,
        .rs1 = 0,
        .rs2 = 0,
        .imm = 0,
        .mask = 0,
        .fatptr_imm = {0, 0},
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
