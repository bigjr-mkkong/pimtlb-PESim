#include "PESim.h"

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

bool SimdSim::empty_program(){
    return program_.empty();
}

void SimdSim::run(size_t max_cycles) {
    cpu_.load_program(program_);
    // cpu_.load_trace(trace_);
    cpu_.run(max_cycles);
}

SimdCpu &SimdSim::cpu() {
    return cpu_;
}

const SimdCpu &SimdSim::cpu() const {
    return cpu_;
}

SimdMemory &SimdSim::memory() {
    return memory_;
}

const SimdMemory &SimdSim::memory() const {
    return memory_;
}

void SimdSim::add_region(uint32_t varidx, size_t size_bytes) {
    memory_.add_region(varidx, size_bytes);
}

void SimdSim::load_program(const std::vector<SimdInstruction> &program) {
    program_ = program;
    cpu_.load_program(program_);
}

void SimdSim::load_trace(const std::priority_queue<ext_sig_t> &trace){
    trace_ = trace;
    cpu_.load_trace(trace_);
}
