#include "PESim.h"
#include "memory_system.h"
#include <cassert>
#include <memory>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#define MEM_BATCH_SZ    1

void SimdSim::dramsim3_read_callback(uint64_t addr) {
    pendmap[addr] -= 1;
    if(pendmap[addr] < 0){
        std::cerr<<"dramsim3 read callback failed"<<std::endl;
        exit(1);
    }
    return;
}

void SimdSim::dramsim3_write_callback(uint64_t addr) {
    pendmap[addr] -= 1;
    if(pendmap[addr] < 0){
        std::cerr<<"dramsim3 write callback failed"<<std::endl;
        exit(1);
    }
    return;
}

bool SimdSim::dramsim3_empty(){
    for(auto &i: pendmap) {
        if(i.second > 0)
            return false;
    }

    return true;
}


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
    
    dramsim3 = std::make_unique<dramsim3::MemorySystem>(
        "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/dramsim3/configs",
        "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/output",
        [this](uint64_t addr) {this->dramsim3_read_callback(addr);},
        [this](uint64_t addr) {this->dramsim3_write_callback(addr);}
    );
}

bool SimdSim::empty_program(){
    return program_.empty();
}

void SimdSim::run_MEM(sim_option_t opt){
    size_t i = 0;
    bool early_stop = false;
    for(;i < opt.max_cycle; i++) {
        trace_ent_t tr = traces_.top();
        bool is_write = (tr.op == WRITE);
        if(i >= tr.time) {
            if(dramsim3->WillAcceptTransaction(tr.addr, is_write)){
                dramsim3->AddTransaction(tr.addr, is_write, false);
                traces_.pop();
            }
        }

        if(dramsim3_empty() && traces_.empty()){
            early_stop = true;
            break;
        }
    }
    if(early_stop){
        std::cout<<"MEM Simulation finished before time runs out"<<std::endl;
    }
    std::cout<<"MEM Simulation done in cycle: "<<i<<std::endl;
}
void SimdSim::run_PIM(sim_option_t opt){
    size_t i = 0;
    bool early_stop = false;
    for(;i < opt.max_cycle; i++) {
        cpu_.tick();
        cpu_.inc_cycl();

        if(cpu_.is_stopped()){
            early_stop = true;
            break;
        }
    }

    if(early_stop){
        std::cout<<"PIM Simulation finished before time runs out"<<std::endl;
    }

    std::cout<<"PIM Simulation done in cycle: "<<i<<std::endl;

}
void SimdSim::run_HYBRID(sim_option_t opt){
    size_t cycl = 0;
    cpu_.load_program(program_);

    bool pe_fin = false, trace_fin = false;
    if(traces_.empty()){
       std::cout<<"Trace is empty, this simulation will run without stop"<<std::endl;
    }

    enum SimMode {
        MEM_WAIT_BATCH,
        MEM_RUN,
        SWITCH_TO_PIM,
        PIM_RUN,
        SWITCH_TO_MEM
    };

    bool pimcpu_started = false;

    SimMode sim_mode = MEM_WAIT_BATCH;

    int mem_budget_cycles = 0;
    int pim_budget_cycles = 0;
    int batch_left = MEM_BATCH_SZ;

    bool batch_started = false;

    for (size_t i = 0; i < opt.max_cycle; ++i) {

        switch (sim_mode) {

        case MEM_WAIT_BATCH: {

            if (!traces_.empty()) {
                trace_ent_t tr = traces_.top();

                if (tr.time <= i) {
                    bool is_write = (tr.op != READ);

                    if (dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                        dramsim3->AddTransaction(tr.addr, is_write, false);
                        pendmap[tr.addr] += 1;
                        traces_.pop();

                        batch_left = MEM_BATCH_SZ - 1;
                        batch_started = true;
                        mem_budget_cycles = 0;

                        sim_mode = MEM_RUN;
                    }
                }
            }
            break;
        }

        case MEM_RUN: {
            if (batch_left > 0 && !traces_.empty()) {
                trace_ent_t tr = traces_.top();

                if (tr.time <= i) {
                    bool is_write = (tr.op != READ);

                    if (dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                        dramsim3->AddTransaction(tr.addr, is_write, false);
                        pendmap[tr.addr] += 1;

                        traces_.pop();
                        batch_left--;
                    }
                }
            }

            dramsim3->ClockTick();
            mem_budget_cycles++;

            if (batch_started && dramsim3_empty()) {
                pim_budget_cycles = 0;
                sim_mode = SWITCH_TO_PIM;

                if (pimcpu_started) {
                    cpu_.resume();
                }
            }

            break;
        }

        case SWITCH_TO_PIM: {
            if (!pimcpu_started) {
                pimcpu_started = true;
                sim_mode = PIM_RUN;
            } else {
                cpu_.tick();
                cpu_.inc_cycl();

                if (cpu_.ready4signal()) {
                    sim_mode = PIM_RUN;
                }
            }
            break;
        }

        case PIM_RUN: {
            cpu_.tick();
            cpu_.inc_cycl();
            pimcpu_started = true;
            pim_budget_cycles++;

            if (pim_budget_cycles >= mem_budget_cycles) {
                cpu_.pause();
                sim_mode = SWITCH_TO_MEM;
            }
            break;
        }

        case SWITCH_TO_MEM: {
            cpu_.tick();
            cpu_.inc_cycl();

            if (cpu_.ready4signal()) {
                batch_left = MEM_BATCH_SZ;
                batch_started = false;
                mem_budget_cycles = 0;
                pim_budget_cycles = 0;
                sim_mode = MEM_WAIT_BATCH;
            }
            break;
        }

        default:
            break;
        }

        if (cpu_.is_stopped() && !pe_fin) {
            std::cout << "Program finished at: " << i << std::endl;
            pe_fin = true;
        }

        if (traces_.empty() && !trace_fin) {
            std::cout << "Trace finished at: " << i << std::endl;
            trace_fin = true;
        }

        if (pe_fin && trace_fin) {
            std::cout << "HYBRID Simulation done in cycle: " << i << std::endl;
            break;
        }
    }

    if(cycl == opt.max_cycle - 1){
        std::cout<<"HYBRID Simulation finished before worload runs out, did you give it enough time?"<<std::endl;
    }
}

void SimdSim::run(sim_option_t opt) {
    switch(opt.mode){
        case sim_mode::MEM:
            run_MEM(opt);
            break;
        case sim_mode::PIM:
            run_PIM(opt);
            break;
        case sim_mode::HYBRID:
            run_HYBRID(opt);
            break;
    }

    return;
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

void SimdSim::fill_region(uint32_t varidx, const std::vector<uint8_t> &src){
    memory_.fill_region(varidx, src);
}

void SimdSim::set_vreg(size_t idx, const std::array<uint32_t, 4> &value){
    cpu_.set_vreg(idx, value);
}

void SimdSim::load_program(const std::vector<SimdInstruction> &program) {
    program_ = program;
    cpu_.load_program(program_);
}

void SimdSim::load_trace(const std::priority_queue<trace_ent_t> &traces) {
    traces_ = traces;
    cpu_.load_trace(traces_);
}
